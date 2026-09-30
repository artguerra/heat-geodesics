/* global HeatGeodesicsModule */

(async () => {
  const canvas = document.getElementById("canvas");
  const status = document.getElementById("status");
  const sourceCount = document.getElementById("source-count");
  const loadingOverlay = document.getElementById("loading-overlay");
  const loadingMessage = document.getElementById("loading-message");
  const module = await HeatGeodesicsModule({ canvas });

  const call = (name, result, arguments_) =>
    module.cwrap(name, result, arguments_);

  const api = {
    load: call("app_load_mesh", "number", ["string"]),
    clear: call("app_clear_sources", null, []),
    add: call("app_add_source", "number", ["number"]),
    sources: call("app_source_count", "number", []),
    vertices: call("app_vertex_count", "number", []),
    faces: call("app_face_count", "number", []),
    vertex: call("app_vertex_coordinate", "number", ["number", "number"]),
    face: call("app_face_index", "number", ["number", "number"]),
    source: call("app_is_source", "number", ["number"]),
    heat: call("app_run_heat", "number", ["number"]),
    distance: call("app_compute_distance", "number", []),
    field: call("app_field_value", "number", ["number", "number"]),
    error: call("app_last_error", "string", []),
  };

  const gl = canvas.getContext("webgl2", { antialias: true });
  if (!gl) {
    status.textContent = "WebGL 2 is required.";
    return;
  }

  const meshProgram = createProgram(
    gl,
    `#version 300 es
      in vec3 position;
      in vec3 normal;
      in vec3 color;
      in float fieldValue;

      uniform float yaw;
      uniform float pitch;
      uniform float aspect;
      uniform float zoom;

      out vec3 vertexColor;
      out float lighting;
      out float scalarValue;

      vec3 rotateToView(vec3 value) {
        float cy = cos(yaw);
        float sy = sin(yaw);
        float cx = cos(pitch);
        float sx = sin(pitch);

        value = vec3(
          cy * value.x + sy * value.z,
          value.y,
          -sy * value.x + cy * value.z
        );
        return vec3(
          value.x,
          cx * value.y - sx * value.z,
          sx * value.y + cx * value.z
        );
      }

      void main() {
        vec3 viewPosition = rotateToView(position);
        vec3 viewNormal = normalize(rotateToView(normal));
        vec3 lightDirection = normalize(vec3(-0.6, 0.8, 1.0));
        float perspective = 2.9 - viewPosition.z;

        gl_Position = vec4(
          zoom * 1.65 * aspect * viewPosition.x / perspective,
          zoom * 1.65 * viewPosition.y / perspective,
          -viewPosition.z / 3.2,
          1.0
        );

        vertexColor = color;
        lighting = 0.35 + 0.65 * max(dot(viewNormal, lightDirection), 0.0);
        scalarValue = fieldValue;
      }`,
    `#version 300 es
      precision highp float;

      in vec3 vertexColor;
      in float lighting;
      in float scalarValue;

      uniform bool showDistanceBands;
      out vec4 outputColor;

      void main() {
        float alternatingBand = step(0.5, fract(scalarValue * 12.0));
        float bandShade = showDistanceBands
          ? mix(0.68, 1.0, alternatingBand)
          : 1.0;
        outputColor = vec4(vertexColor * lighting * bandShade, 1.0);
      }`,
  );

  const sourceProgram = createProgram(
    gl,
    `#version 300 es
      in vec3 position;

      uniform float yaw;
      uniform float pitch;
      uniform float aspect;
      uniform float zoom;

      vec3 rotateToView(vec3 value) {
        float cy = cos(yaw);
        float sy = sin(yaw);
        float cx = cos(pitch);
        float sx = sin(pitch);

        value = vec3(
          cy * value.x + sy * value.z,
          value.y,
          -sy * value.x + cy * value.z
        );
        return vec3(
          value.x,
          cx * value.y - sx * value.z,
          sx * value.y + cx * value.z
        );
      }

      void main() {
        vec3 viewPosition = rotateToView(position);
        float perspective = 2.9 - viewPosition.z;

        gl_Position = vec4(
          zoom * 1.65 * aspect * viewPosition.x / perspective,
          zoom * 1.65 * viewPosition.y / perspective,
          -viewPosition.z / 3.2,
          1.0
        );
        gl_PointSize = 22.0;
      }`,
    `#version 300 es
      precision highp float;
      out vec4 outputColor;

      void main() {
        float radius = length(gl_PointCoord * 2.0 - 1.0);
        if (radius > 1.0) {
          discard;
        }

        vec3 fill = vec3(1.0, 0.18, 0.06);
        vec3 border = vec3(1.0, 0.92, 0.72);
        outputColor = vec4(mix(fill, border, smoothstep(0.68, 0.82, radius)), 1.0);
      }`,
  );

  const positionBuffer = gl.createBuffer();
  const normalBuffer = gl.createBuffer();
  const colorBuffer = gl.createBuffer();
  const fieldBuffer = gl.createBuffer();
  const triangleBuffer = gl.createBuffer();
  const sourceBuffer = gl.createBuffer();

  let positions = new Float32Array();
  let normals = new Float32Array();
  let triangles = new Uint32Array();
  let sourceIndices = new Uint32Array();
  let yaw = 0.55;
  let pitch = -0.22;
  let zoom = 1;
  let mode = "none";

  function setStatus(message, error = false) {
    status.textContent = message;
    status.style.color = error ? "#ff8e8e" : "#c2cad7";
  }

  function setLoading(loading, message = "Loading…") {
    loadingMessage.textContent = message;
    loadingOverlay.hidden = !loading;
  }

  function nextFrame() {
    return new Promise((resolve) => requestAnimationFrame(resolve));
  }

  function uploadMesh() {
    const vertexCount = api.vertices();
    const faceCount = api.faces();
    positions = new Float32Array(vertexCount * 3);
    normals = new Float32Array(vertexCount * 3);
    triangles = new Uint32Array(faceCount * 3);

    const lower = [Infinity, Infinity, Infinity];
    const upper = [-Infinity, -Infinity, -Infinity];

    for (let vertex = 0; vertex < vertexCount; vertex += 1) {
      for (let axis = 0; axis < 3; axis += 1) {
        const coordinate = api.vertex(vertex, axis);
        positions[3 * vertex + axis] = coordinate;
        lower[axis] = Math.min(lower[axis], coordinate);
        upper[axis] = Math.max(upper[axis], coordinate);
      }
    }

    const center = lower.map((coordinate, axis) =>
      (coordinate + upper[axis]) / 2,
    );
    const longestSide = Math.max(
      ...upper.map((coordinate, axis) => coordinate - lower[axis]),
    );
    const scale = 1.8 / longestSide;

    for (let vertex = 0; vertex < vertexCount; vertex += 1) {
      for (let axis = 0; axis < 3; axis += 1) {
        positions[3 * vertex + axis] =
          (positions[3 * vertex + axis] - center[axis]) * scale;
      }
    }

    for (let face = 0; face < faceCount; face += 1) {
      const a = api.face(face, 0);
      const b = api.face(face, 1);
      const c = api.face(face, 2);
      triangles.set([a, b, c], 3 * face);
    }

    if (signedMeshVolume() < 0) {
      for (let face = 0; face < faceCount; face += 1) {
        const offset = 3 * face;
        const temporary = triangles[offset + 1];
        triangles[offset + 1] = triangles[offset + 2];
        triangles[offset + 2] = temporary;
      }
    }

    for (let face = 0; face < faceCount; face += 1) {
      const a = triangles[3 * face];
      const b = triangles[3 * face + 1];
      const c = triangles[3 * face + 2];
      accumulateFaceNormal(a, b, c);
    }

    normalizeVertexNormals();

    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, normals, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, triangleBuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, triangles, gl.STATIC_DRAW);

    updateColorsAndSources();
  }

  function signedMeshVolume() {
    let volume = 0;

    for (let face = 0; face < triangles.length / 3; face += 1) {
      const a = 3 * triangles[3 * face];
      const b = 3 * triangles[3 * face + 1];
      const c = 3 * triangles[3 * face + 2];
      volume +=
        positions[a] *
          (positions[b + 1] * positions[c + 2] -
            positions[b + 2] * positions[c + 1]) +
        positions[a + 1] *
          (positions[b + 2] * positions[c] -
            positions[b] * positions[c + 2]) +
        positions[a + 2] *
          (positions[b] * positions[c + 1] -
            positions[b + 1] * positions[c]);
    }

    return volume / 6;
  }

  function accumulateFaceNormal(a, b, c) {
    const ab = [
      positions[3 * b] - positions[3 * a],
      positions[3 * b + 1] - positions[3 * a + 1],
      positions[3 * b + 2] - positions[3 * a + 2],
    ];
    const ac = [
      positions[3 * c] - positions[3 * a],
      positions[3 * c + 1] - positions[3 * a + 1],
      positions[3 * c + 2] - positions[3 * a + 2],
    ];

    const normal = [
      ab[1] * ac[2] - ab[2] * ac[1],
      ab[2] * ac[0] - ab[0] * ac[2],
      ab[0] * ac[1] - ab[1] * ac[0],
    ];

    for (const vertex of [a, b, c]) {
      for (let axis = 0; axis < 3; axis += 1) {
        normals[3 * vertex + axis] += normal[axis];
      }
    }
  }

  function normalizeVertexNormals() {
    for (let vertex = 0; vertex < api.vertices(); vertex += 1) {
      const offset = 3 * vertex;
      const length = Math.hypot(
        normals[offset],
        normals[offset + 1],
        normals[offset + 2],
      ) || 1;

      normals[offset] /= length;
      normals[offset + 1] /= length;
      normals[offset + 2] /= length;
    }
  }

  function updateColorsAndSources() {
    const vertexCount = api.vertices();
    const values = new Float64Array(vertexCount);
    const colors = new Float32Array(vertexCount * 3);
    const normalizedValues = new Float32Array(vertexCount);
    const selected = [];
    let minimum = Infinity;
    let maximum = -Infinity;

    for (let vertex = 0; vertex < vertexCount; vertex += 1) {
      values[vertex] =
        mode === "none"
          ? 0
          : api.field(vertex, mode === "distance" ? 1 : 0);
      minimum = Math.min(minimum, values[vertex]);
      maximum = Math.max(maximum, values[vertex]);

      if (api.source(vertex)) {
        selected.push(vertex);
      }
    }

    const range = maximum - minimum || 1;
    for (let vertex = 0; vertex < vertexCount; vertex += 1) {
      const normalized = (values[vertex] - minimum) / range;
      normalizedValues[vertex] = normalized;
      const color =
        mode === "none"
          ? [0.66, 0.7, 0.76]
          : [
              Math.min(1, 0.12 + 1.1 * normalized),
              0.18 + 0.55 * normalized,
              Math.max(0.08, 0.85 - 0.75 * normalized),
            ];
      colors.set(color, 3 * vertex);
    }

    sourceIndices = new Uint32Array(selected);
    gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, colors, gl.DYNAMIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, fieldBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, normalizedValues, gl.DYNAMIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, sourceBuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, sourceIndices, gl.DYNAMIC_DRAW);
    sourceCount.textContent = `${sourceIndices.length} selected`;
  }

  function setViewUniforms(program, aspect) {
    gl.uniform1f(gl.getUniformLocation(program, "yaw"), yaw);
    gl.uniform1f(gl.getUniformLocation(program, "pitch"), pitch);
    gl.uniform1f(gl.getUniformLocation(program, "aspect"), aspect);
    gl.uniform1f(gl.getUniformLocation(program, "zoom"), zoom);
  }

  function render() {
    const pixelRatio = window.devicePixelRatio || 1;
    const width = Math.round(canvas.clientWidth * pixelRatio);
    const height = Math.round(canvas.clientHeight * pixelRatio);

    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
    }

    gl.viewport(0, 0, width, height);
    gl.clearColor(0.055, 0.065, 0.085, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.enable(gl.DEPTH_TEST);
    gl.enable(gl.CULL_FACE);
    gl.cullFace(gl.BACK);
    gl.frontFace(gl.CCW);

    gl.useProgram(meshProgram);
    setViewUniforms(meshProgram, height / width);
    bindAttribute(gl, meshProgram, "position", positionBuffer);
    bindAttribute(gl, meshProgram, "normal", normalBuffer);
    bindAttribute(gl, meshProgram, "color", colorBuffer);
    bindAttribute(gl, meshProgram, "fieldValue", fieldBuffer, 1);
    gl.uniform1i(
      gl.getUniformLocation(meshProgram, "showDistanceBands"),
      mode === "distance" ? 1 : 0,
    );
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, triangleBuffer);
    gl.drawElements(gl.TRIANGLES, triangles.length, gl.UNSIGNED_INT, 0);

    if (sourceIndices.length > 0) {
      gl.disable(gl.CULL_FACE);
      gl.disable(gl.DEPTH_TEST);
      gl.useProgram(sourceProgram);
      setViewUniforms(sourceProgram, height / width);
      bindAttribute(gl, sourceProgram, "position", positionBuffer);
      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, sourceBuffer);
      gl.drawElements(gl.POINTS, sourceIndices.length, gl.UNSIGNED_INT, 0);
    }

    requestAnimationFrame(render);
  }

  function project(position) {
    const cy = Math.cos(yaw);
    const sy = Math.sin(yaw);
    const cx = Math.cos(pitch);
    const sx = Math.sin(pitch);
    const rotatedX = cy * position[0] + sy * position[2];
    const rotatedZ = -sy * position[0] + cy * position[2];
    const viewY = cx * position[1] - sx * rotatedZ;
    const viewZ = sx * position[1] + cx * rotatedZ;
    const perspective = 2.9 - viewZ;

    return [
      zoom * 1.65 * (canvas.height / canvas.width) * rotatedX / perspective,
      zoom * 1.65 * viewY / perspective,
      viewZ,
    ];
  }

  function pickSource(clientX, clientY) {
    const bounds = canvas.getBoundingClientRect();
    const cursor = [
      2 * (clientX - bounds.left) / bounds.width - 1,
      1 - 2 * (clientY - bounds.top) / bounds.height,
    ];
    const projectedVertices = new Array(api.vertices());
    let selectedFace = -1;
    let frontDepth = -Infinity;

    const projectedVertex = (vertex) => {
      if (!projectedVertices[vertex]) {
        projectedVertices[vertex] = project([
          positions[3 * vertex],
          positions[3 * vertex + 1],
          positions[3 * vertex + 2],
        ]);
      }
      return projectedVertices[vertex];
    };

    for (let face = 0; face < triangles.length / 3; face += 1) {
      const a = projectedVertex(triangles[3 * face]);
      const b = projectedVertex(triangles[3 * face + 1]);
      const c = projectedVertex(triangles[3 * face + 2]);
      const area = edgeFunction(a, b, c);

      if (area <= 0) {
        continue;
      }

      const weightA = edgeFunction(b, c, cursor) / area;
      const weightB = edgeFunction(c, a, cursor) / area;
      const weightC = 1 - weightA - weightB;
      if (weightA < 0 || weightB < 0 || weightC < 0) {
        continue;
      }

      const depth = weightA * a[2] + weightB * b[2] + weightC * c[2];
      if (depth > frontDepth) {
        frontDepth = depth;
        selectedFace = face;
      }
    }

    if (selectedFace >= 0) {
      const candidates = [
        triangles[3 * selectedFace],
        triangles[3 * selectedFace + 1],
        triangles[3 * selectedFace + 2],
      ];
      const selectedVertex = candidates.reduce((closest, candidate) => {
        const current = projectedVertex(candidate);
        const previous = projectedVertex(closest);
        return squaredDistance(current, cursor) < squaredDistance(previous, cursor)
          ? candidate
          : closest;
      });

      api.add(selectedVertex);
      mode = "none";
      updateColorsAndSources();
      setStatus(`Source added at vertex ${selectedVertex}.`);
    }
  }

  function edgeFunction(a, b, point) {
    return (
      (b[0] - a[0]) * (point[1] - a[1]) -
      (b[1] - a[1]) * (point[0] - a[0])
    );
  }

  function squaredDistance(a, b) {
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2;
  }

  const meshSelect = document.getElementById("mesh");
  let currentMeshPath = "/data/bunny_fine.off";

  meshSelect.addEventListener("change", async () => {
    const requestedPath = meshSelect.value;
    const requestedName = meshSelect.options[meshSelect.selectedIndex].text;
    setLoading(true, `Preparing ${requestedName}…`);
    await nextFrame();

    try {
      if (!api.load(requestedPath)) {
        meshSelect.value = currentMeshPath;
        setStatus(api.error(), true);
        return;
      }

      currentMeshPath = requestedPath;
      mode = "none";
      yaw = 0.55;
      pitch = -0.22;
      zoom = 1;
      uploadMesh();
      setStatus(`Loaded ${requestedName}.`);
    } catch (error) {
      meshSelect.value = currentMeshPath;
      setStatus(`Could not load ${requestedName}.`, true);
      console.error(error);
    } finally {
      setLoading(false);
    }
  });

  document.getElementById("clear").addEventListener("click", () => {
    api.clear();
    mode = "none";
    updateColorsAndSources();
    setStatus("Sources cleared.");
  });

  document.getElementById("distance").addEventListener("click", async () => {
    setLoading(true, "Computing geodesic distance…");
    await nextFrame();
    try {
      if (api.distance()) {
        mode = "distance";
        updateColorsAndSources();
        setStatus("Geodesic distance field computed.");
      } else {
        setStatus(api.error(), true);
      }
    } catch (error) {
      setStatus("Distance computation failed.", true);
      console.error(error);
    } finally {
      setLoading(false);
    }
  });

  document.getElementById("heat").addEventListener("click", async () => {
    const duration = Number(document.getElementById("duration").value);
    setLoading(true, "Simulating heat flow…");
    await nextFrame();
    try {
      if (api.heat(duration)) {
        mode = "heat";
        updateColorsAndSources();
        setStatus("Heat simulation completed.");
      } else {
        setStatus(api.error(), true);
      }
    } catch (error) {
      setStatus("Heat simulation failed.", true);
      console.error(error);
    } finally {
      setLoading(false);
    }
  });

  let pointerInteraction;

  canvas.addEventListener("pointerdown", (event) => {
    if (event.button !== 0 || pointerInteraction) {
      return;
    }

    pointerInteraction = {
      id: event.pointerId,
      x: event.clientX,
      y: event.clientY,
      yaw,
      pitch,
      dragging: false,
    };
    canvas.setPointerCapture(event.pointerId);
  });

  canvas.addEventListener("pointermove", (event) => {
    if (!pointerInteraction || event.pointerId !== pointerInteraction.id) {
      return;
    }

    const deltaX = event.clientX - pointerInteraction.x;
    const deltaY = event.clientY - pointerInteraction.y;
    if (!pointerInteraction.dragging && Math.hypot(deltaX, deltaY) < 5) {
      return;
    }

    pointerInteraction.dragging = true;
    yaw = pointerInteraction.yaw + deltaX * 0.008;
    pitch = Math.max(
      -1.4,
      Math.min(1.4, pointerInteraction.pitch + deltaY * 0.008),
    );
  });

  const finishPointerInteraction = (event) => {
    if (!pointerInteraction || event.pointerId !== pointerInteraction.id) {
      return;
    }

    if (!pointerInteraction.dragging && event.type === "pointerup") {
      pickSource(event.clientX, event.clientY);
    }

    if (canvas.hasPointerCapture(event.pointerId)) {
      canvas.releasePointerCapture(event.pointerId);
    }
    pointerInteraction = undefined;
  };

  canvas.addEventListener("pointerup", finishPointerInteraction);
  canvas.addEventListener("pointercancel", finishPointerInteraction);
  canvas.addEventListener("lostpointercapture", (event) => {
    if (pointerInteraction?.id === event.pointerId) {
      pointerInteraction = undefined;
    }
  });

  canvas.addEventListener(
    "wheel",
    (event) => {
      event.preventDefault();
      zoom = Math.min(4, Math.max(0.35, zoom * Math.exp(-event.deltaY * 0.001)));
    },
    { passive: false },
  );

  setLoading(true, "Preparing Bunny…");
  await nextFrame();
  if (api.load("/data/bunny_fine.off")) {
    uploadMesh();
    setStatus("Bunny loaded. Click a vertex to add a source.");
    render();
  } else {
    setStatus(api.error(), true);
  }
  setLoading(false);
})();

function createProgram(gl, vertexSource, fragmentSource) {
  const compileShader = (type, source) => {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      throw new Error(gl.getShaderInfoLog(shader));
    }
    return shader;
  };

  const program = gl.createProgram();
  gl.attachShader(program, compileShader(gl.VERTEX_SHADER, vertexSource));
  gl.attachShader(program, compileShader(gl.FRAGMENT_SHADER, fragmentSource));
  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    throw new Error(gl.getProgramInfoLog(program));
  }
  return program;
}

function bindAttribute(gl, program, name, buffer, size = 3) {
  const location = gl.getAttribLocation(program, name);
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
  gl.enableVertexAttribArray(location);
  gl.vertexAttribPointer(location, size, gl.FLOAT, false, 0, 0);
}
