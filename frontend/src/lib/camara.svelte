<script lang="ts">
  import { onMount, onDestroy } from 'svelte';
  import { browser } from '$app/environment';

  // --- ESTADOS OPTIMIZADOS (Svelte 5) ---
  let videoElement = $state<HTMLVideoElement | null>(null);
  let status = $state("SYSTEM_BOOTING...");
  let authorizedUser = $state<any>(null);
  let isProcessing = false; 
  let blinkCount = $state(0);
  let isBlinking = false;
  
  let precision = $state("0.0000000");
  let facePos = $state({ x: 0, y: 0, w: 0, h: 0 });
  let userThumb = $state<string | null>(null);

  // --- TERMINAL DE LOGS ---
  let logs = $state<{msg: string, time: string, type: 'info' | 'warn' | 'success'}[]>([]);

  function addLog(msg: string, type: 'info' | 'warn' | 'success' = 'info') {
    const time = new Date().toLocaleTimeString('en-GB', { hour12: false });
    logs = [{ msg, time, type }, ...logs].slice(0, 8);
  }

  // --- MOTOR WASM Y RENDIMIENTO ---
  let engine: any;
  let wasmModule: any;
  let sharedBufferPtr: number;
  const DIMS = 478 * 3;
  let videoWidth = 0;
  let videoHeight = 0;

  let canvasThumb: HTMLCanvasElement;
  let ctxThumb: CanvasRenderingContext2D | null;

  // --- REGISTRO ---
  let isRegistering = $state(false);
  let rawPoseSamples: Float32Array[] = [];
  let anglesDone = $state({ left: false, right: false, up: false, down: false });

  // --- INTEGRACIÓN BACKEND (SQLite Sync) ---
  
  async function fetchBiometricDB() {
    try {
      const res = await fetch('/api/get-biometrics');
      if (!res.ok) throw new Error();
      const data = await res.json();
      // Mapeamos los datos asegurando que el modelo sea un objeto JSON
      return data.map((u: any) => ({
        ...u,
        model: typeof u.model === 'string' ? JSON.parse(u.model) : u.model
      }));
    } catch (e) {
      addLog("DB_FETCH_FAILED", "warn");
      return [];
    }
  }

  async function syncProfileToServer(userData: any) {
    try {
      const res = await fetch('/api/save-localstorage', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          key: `BIO_${userData.id}`,
          value: JSON.stringify(userData)
        })
      });
      return res.ok;
    } catch (e) {
      return false;
    }
  }

  async function loadWasmEngine() {
    const scriptPath = '/camara.js';
    const wasmGlue = await import(/* @vite-ignore */ scriptPath);
    return await wasmGlue.default({ locateFile: (path: string) => `/${path}` });
  }

  onMount(async () => {
    if (!browser) return;

    canvasThumb = document.createElement('canvas');
    canvasThumb.width = 100; canvasThumb.height = 100;
    ctxThumb = canvasThumb.getContext('2d', { alpha: false, desynchronized: true });

    try {
      status = "LOADING_WASM_CORE...";
      wasmModule = await loadWasmEngine();
      engine = new wasmModule.BiometricEngine();
      sharedBufferPtr = wasmModule._malloc(DIMS * 4);

      const mp = await import("@mediapipe/tasks-vision");
      const files = await mp.FilesetResolver.forVisionTasks('/mediapipe');
      const faceLandmarker = await mp.FaceLandmarker.createFromOptions(files, {
        baseOptions: { modelAssetPath: `/mediapipe/face_landmarker.task`, delegate: "GPU" },
        outputFaceBlendshapes: true,
        runningMode: "VIDEO",
        numFaces: 1
      });

      status = "PROTOCOL_ALPHA_ACTIVE";
      addLog("ENGINE_LOADED", "info");
      startCamera(faceLandmarker);
    } catch (e) {
      status = "CORE_FAILURE";
      addLog("CRITICAL_WASM_ERROR", "warn");
    }
  });

  async function startCamera(faceLandmarker: any) {
    const stream = await navigator.mediaDevices.getUserMedia({ 
      video: { width: 1280, height: 720, frameRate: { ideal: 30 } } 
    });
    if (videoElement) {
      videoElement.srcObject = stream;
      videoElement.onloadedmetadata = () => {
        videoWidth = videoElement!.clientWidth;
        videoHeight = videoElement!.clientHeight;
        videoElement?.play();
        predict(faceLandmarker);
      };
    }
  }

  function captureThumb(): string {
    if (ctxThumb && videoElement) {
      ctxThumb.drawImage(videoElement, 490, 210, 300, 300, 0, 0, 100, 100);
      return canvasThumb.toDataURL('image/webp', 0.7);
    }
    return "";
  }

  function predict(faceLandmarker: any) {
    if (!videoElement || !wasmModule || !sharedBufferPtr) return;
    
    const results = faceLandmarker.detectForVideo(videoElement, performance.now());
    
    if (results.faceLandmarks?.[0]) {
      const lms = results.faceLandmarks[0];
      let minX = 1, minY = 1, maxX = 0, maxY = 0;
      const heap = new Float32Array(wasmModule.HEAPF32.buffer, sharedBufferPtr, DIMS);

      for (let i = 0; i < 478; i++) {
        const pt = lms[i];
        if (pt.x < minX) minX = pt.x; if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y; if (pt.y > maxY) maxY = pt.y;
        
        const offset = i * 3;
        heap[offset] = pt.x; 
        heap[offset + 1] = pt.y; 
        heap[offset + 2] = pt.z;
      }
      
      facePos = {
        x: (1 - minX) * videoWidth - ((maxX - minX) * videoWidth),
        y: minY * videoHeight,
        w: (maxX - minX) * videoWidth,
        h: (maxY - minY) * videoHeight
      };

      const analysis = engine.processFrame(sharedBufferPtr, 478);
      
      const shapes = results.faceBlendshapes[0]?.categories;
      if (shapes) {
        let blinkScore = 0;
        for (let i = 0; i < shapes.length; i++) {
            if (shapes[i].categoryName === 'eyeBlinkLeft' || shapes[i].categoryName === 'eyeBlinkRight') {
                blinkScore += shapes[i].score;
            }
        }
        const eyeBlink = (blinkScore / 2) > 0.45;
        
        if (eyeBlink && !isBlinking) {
          isBlinking = true; blinkCount++;
          addLog(`LIVENESS_BEEP_${blinkCount}`, "info");
          if (blinkCount >= 2) {
            if (isRegistering) finalizeRegistration();
            else if (!authorizedUser) checkBiometrics();
          }
        } else if (!eyeBlink) isBlinking = false;
      }

      if (isRegistering) handlePoseCapture(analysis.yaw, analysis.pitch, heap);
    }
    requestAnimationFrame(() => predict(faceLandmarker));
  }

  function handlePoseCapture(yaw: number, pitch: number, heap: Float32Array) {
    const cap = (key: keyof typeof anglesDone) => {
      rawPoseSamples.push(new Float32Array(heap));
      if (rawPoseSamples.length % 60 === 0) {
        anglesDone[key] = true;
        addLog(`VECTOR_${key.toUpperCase()}_CAPTURED`, "success");
      }
    };
    if (!anglesDone.left && yaw < -0.05) cap('left');
    else if (!anglesDone.right && yaw > 0.05) cap('right');
    else if (!anglesDone.up && pitch > 0.05) cap('up');
    else if (!anglesDone.down && pitch < -0.05) cap('down');
  }

  async function checkBiometrics() {
    if (isProcessing) return;
    isProcessing = true;
    status = "QUERYING_SQLITE_VAULT...";
    
    // CAMBIO: Obtenemos la base de datos desde el Backend
    const db = await fetchBiometricDB();
    
    if (db.length === 0) {
      status = "NO_PROFILES_FOUND";
      addLog("DATABASE_EMPTY", "warn");
      isProcessing = false;
      return;
    }

    let match = null;
    let minDistance = 999;

    for (const user of db) {
      const mPtr = wasmModule._malloc(DIMS * 4);
      const iPtr = wasmModule._malloc(DIMS * 4);
      wasmModule.HEAPF32.set(new Float32Array(user.model.mean), mPtr / 4);
      wasmModule.HEAPF32.set(new Float32Array(user.model.invCovariance), iPtr / 4);

      const dist = engine.calculateMahalanobis(sharedBufferPtr, mPtr, iPtr, DIMS);
      if (dist < minDistance) { minDistance = dist; match = user; }
      wasmModule._free(mPtr); wasmModule._free(iPtr);
    }

    const rawScore = engine.getConfidenceScore(minDistance);
    precision = rawScore.toFixed(7);

    if (match && rawScore >= 99.85) {
      authorizedUser = match;
      userThumb = match.thumb;
      status = "ACCESS_GRANTED_ALPHA";
      addLog(`AUTH_SUCCESS: ${precision}%`, "success");
    } else {
      authorizedUser = null;
      status = rawScore > 99.0 ? "MARGINAL_CONFIDENCE_REJECT" : "SECURITY_BREACH_DETECTED";
      addLog(`REJECTED: ${precision}%`, "warn");
    }
    isProcessing = false;
  }

  async function finalizeRegistration() {
    status = "ENCRYPTING_VAULT...";
    const tPtr = wasmModule._malloc(rawPoseSamples.length * DIMS * 4);
    const tHeap = new Float32Array(wasmModule.HEAPF32.buffer, tPtr, rawPoseSamples.length * DIMS);
    rawPoseSamples.forEach((v, i) => tHeap.set(v, i * DIMS));

    const model = engine.trainModel(tPtr, rawPoseSamples.length, DIMS, crypto.randomUUID());
    
    const newUser = { 
      id: model.userId, 
      thumb: captureThumb(), 
      model: { 
        mean: Array.from(model.mean), 
        invCovariance: Array.from(model.invCovariance) 
      } 
    };

    // CAMBIO: Sincronización persistente con SQLite
    const success = await syncProfileToServer(newUser);

    wasmModule._free(tPtr);
    rawPoseSamples = []; 
    isRegistering = false; 

    if (success) {
      status = "VAULT_SEALED_SQLITE";
      addLog("REMOTE_PROFILE_SAVED", "success");
    } else {
      status = "SYNC_FAILED_LOCAL_ONLY";
      addLog("BACKEND_OFFLINE", "warn");
      // Fallback a localStorage por si acaso
      localStorage.setItem("BIO_BACKUP", JSON.stringify(newUser));
    }
  }

  onDestroy(() => { if (wasmModule && sharedBufferPtr) wasmModule._free(sharedBufferPtr); });
</script>

<main class="scanner" role="main">
  <video bind:this={videoElement} aria-label="Escáner biométrico" muted playsinline></video>

  {#if authorizedUser}
    <div class="face-tag" style="left: {facePos.x}px; top: {facePos.y - 130}px; width: {facePos.w}px;">
      <div class="tag-content">
        <div class="thumb-container">
           <img src={userThumb} alt="Thumbnail de usuario autorizado" />
           <div class="scan-line"></div>
        </div>
        <div class="tag-data">
          <span class="user-id">ID: ALPHA_{authorizedUser.id.slice(0,4)}</span>
          <span class="conf" class:high-safety={parseFloat(precision) >= 99.85}>
            {precision}% MATCH
          </span>
          <div class="accuracy-bar-container" role="progressbar" aria-valuenow={precision} aria-valuemin="0" aria-valuemax="100">
            <div 
              class="accuracy-fill" 
              style="width: {precision}%; background: {parseFloat(precision) >= 99.85 ? '#0ff' : '#f00'};"
            ></div>
          </div>
        </div>
      </div>
      <div class="tag-line"></div>
    </div>
  {/if}

  <section class="side-terminal" aria-label="Terminal de logs del sistema">
    <div class="terminal-header">ALPHA_LOG_STREAM</div>
    <div class="logs-container">
      {#each logs as log (log.time + log.msg)}
        <div class="log-entry {log.type}">
          <span class="log-time">[{log.time}]</span>
          <span class="log-msg">{log.msg}</span>
        </div>
      {/each}
    </div>
  </section>

  <nav class="ui">
    <div class="status" class:active={isProcessing} class:denied={status.includes('REJECT') || status.includes('BREACH')}>
      {status}
    </div>
    <div class="hud">
      <div class="item">LIVENESS: {blinkCount}/2</div>
      {#if authorizedUser}
        <div class="item match">IDENTITY_CONFIRMED</div>
      {/if}
    </div>
    {#if !isRegistering && !authorizedUser}
      <button onclick={() => { isRegistering = true; status = "ROTATE_HEAD_FOR_CAPTURE"; blinkCount = 0; logs = []; }}>
        INITIALIZE_ENROLLMENT
      </button>
    {/if}
  </nav>
</main>

<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');

  .scanner { position: relative; width: 100vw; height: 100vh; background: #000; overflow: hidden; }
  video { width: 100%; height: 100%; object-fit: cover; filter: grayscale(1) brightness(0.6) contrast(1.3); transform: scaleX(-1); }

  .side-terminal {
    position: absolute; right: 25px; top: 50%; transform: translateY(-50%);
    width: 260px; background: rgba(0, 15, 15, 0.9); border-left: 2px solid #0ff;
    font-family: 'Share Tech Mono', monospace; padding: 12px; z-index: 20; backdrop-filter: blur(8px);
  }
  .terminal-header { font-size: 0.65rem; color: #0ff; border-bottom: 1px solid rgba(0, 255, 255, 0.2); margin-bottom: 10px; padding-bottom: 5px; letter-spacing: 2px; }
  .log-entry { font-size: 0.6rem; margin-bottom: 6px; display: flex; gap: 8px; animation: logSlide 0.2s ease-out; }
  .log-time { color: #555; }
  .info { color: #0ff; }
  .warn { color: #f00; }
  .success { color: #0f0; }

  .face-tag { position: absolute; pointer-events: none; transition: all 0.04s linear; display: flex; flex-direction: column; align-items: center; z-index: 10; }
  .tag-content { background: rgba(0, 15, 20, 0.95); border: 1px solid #0ff; backdrop-filter: blur(10px); display: flex; gap: 12px; padding: 10px; box-shadow: 0 0 30px rgba(0, 255, 255, 0.3); }
  .thumb-container { position: relative; width: 50px; height: 50px; border: 1px solid #0ff; overflow: hidden; }
  .thumb-container img { width: 100%; height: 100%; object-fit: cover; filter: grayscale(1) cyan; }
  .scan-line { position: absolute; width: 100%; height: 2px; background: #0ff; box-shadow: 0 0 10px #0ff; animation: scanLine 2s infinite linear; }
  .tag-data { display: flex; flex-direction: column; font-family: 'Share Tech Mono', monospace; width: 150px; }
  .user-id { color: #888; font-size: 10px; letter-spacing: 1px; }
  .conf { color: #ff0; font-size: 11px; margin-top: 2px; }
  .high-safety { color: #fff !important; text-shadow: 0 0 10px #0ff; }
  .accuracy-bar-container { width: 100%; height: 4px; background: #111; margin-top: 6px; }
  .accuracy-fill { height: 100%; transition: width 0.05s linear; }
  .tag-line { width: 1px; height: 60px; background: linear-gradient(to bottom, #0ff, transparent); }

  .ui { position: absolute; bottom: 60px; left: 50%; transform: translateX(-50%); display: flex; flex-direction: column; align-items: center; gap: 20px; z-index: 5; }
  .status { font-family: 'Share Tech Mono', monospace; font-size: 1.3rem; color: #0ff; text-shadow: 0 0 10px #0ff; letter-spacing: 3px; }
  .status.active { animation: pulse 0.8s infinite; }
  .status.denied { color: #f00; }
  .hud { display: flex; gap: 20px; background: rgba(0, 10, 10, 0.8); padding: 12px 25px; border-left: 3px solid #0ff; font-family: 'Share Tech Mono', monospace; color: #0ff; font-size: 0.75rem; }
  .match { color: #0f0; font-weight: bold; border-left: 1px solid #0f0; padding-left: 20px; }
  button { background: rgba(0, 255, 255, 0.05); border: 1px solid #0ff; color: #0ff; padding: 14px 40px; cursor: pointer; font-family: 'Share Tech Mono', monospace; transition: 0.4s; letter-spacing: 2px; }
  button:hover { background: #0ff; color: #000; box-shadow: 0 0 40px #0ff; }

  @keyframes scanLine { 0% { top: -10%; } 100% { top: 110%; } }
  @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }
  @keyframes logSlide { from { opacity: 0; transform: translateX(20px); } to { opacity: 1; transform: translateX(0); } }
</style>