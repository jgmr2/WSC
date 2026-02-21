<script lang="ts">
  import { onMount } from 'svelte';
  import * as faceapi from '@vladmandic/face-api';

  // --- ESTADOS CON RUNES (SVELTE 5) ---
  let videoElement: HTMLVideoElement = $state();
  let status = $state("SISTEMA INICIANDO...");
  let points = $state([]); 
  let eyesLocked = $state(false);
  let blinkCount = $state(0);
  let authorizedUser = $state(null);
  let lastEar = 0; 
  let faceBox = $state({ x: 0, y: 0, w: 0, h: 0 }); 
  let leftCircle = $state({ x: 38, y: 45, active: false });
  let rightCircle = $state({ x: 62, y: 45, active: false });

  // --- INTEGRACIÓN DEL MOTOR WASM ---
  const wasmPath = '/camara.js';
  let wasmEngine: any = null; // Guardaremos la instancia aquí

  // Cargamos el módulo
  const motorPromise = import(/* @vite-ignore */ wasmPath)
    .then(m => m.default({ locateFile: p => `/${p}` }));

  const DB_KEY = "BIOMETRIC_LOCAL_DB";

  /**
   * UTILIDAD PARA PASAR DATOS A C++
   * Convierte los puntos de JS en un puntero del HEAP de WASM
   */
  function getWasmHash(currentPoints: any[]) {
    if (!wasmEngine) return null;

    // 1. Crear un Float32Array plano [x1, y1, x2, y2...]
    const flatCoords = new Float32Array(currentPoints.flatMap(p => [p.x, p.y]));
    
    // 2. Reservar memoria en el motor (4 bytes por float)
    const ptr = wasmEngine._malloc(flatCoords.length * 4);
    
    // 3. Escribir los datos en el HEAP
    wasmEngine.HEAPF32.set(flatCoords, ptr >> 2);

    // 4. Llamar a tu función de C++ (ajusta el nombre según tu .cpp)
    // Asumimos que genera un string o un valor numérico único
    const hash = wasmEngine.generate_geometric_hash(ptr, currentPoints.length);

    // 5. Liberar memoria para evitar fugas
    wasmEngine._free(ptr);
    
    return hash;
  }

  async function handleBiometricCheck() {
    if (!wasmEngine) {
        status = "ERROR: MOTOR NO LISTO";
        return;
    }

    status = "PROCESANDO WASM...";
    
    // Generamos la firma usando C++
    const currentHash = getWasmHash(points);
    
    const rawData = localStorage.getItem(DB_KEY);
    const db = rawData ? JSON.parse(rawData) : [];
    
    // COMPARACIÓN VÍA WASM
    // Usamos el motor para comparar la firma actual con las guardadas
    const match = db.find(user => {
        // Asumimos que tu motor tiene una función de similitud
        const score = wasmEngine.get_similarity(currentHash, user.hash);
        return score > 0.85; // Umbral de precisión
    });

    if (match) {
      authorizedUser = match;
      status = "ACCESO AUTORIZADO (WASM)";
    } else {
      const thumb = await captureThumb();
      
      const newUser = {
        name: `SUJETO_${db.length + 101}`,
        hash: currentHash, // El hash generado por C++
        photo: thumb,
        timestamp: new Date().toLocaleString()
      };

      db.push(newUser);
      localStorage.setItem(DB_KEY, JSON.stringify(db));
      authorizedUser = newUser;
      status = "NUEVA IDENTIDAD REGISTRADA";
    }
  }

  // --- CARGA Y PROCESAMIENTO ---

  async function loadIA() {
    try {
      // Esperamos a que el motor WASM esté instanciado
      wasmEngine = await motorPromise;
      
      await faceapi.nets.tinyFaceDetector.loadFromUri('/models');
      await faceapi.nets.faceLandmark68Net.loadFromUri('/models');
      status = "MOTOR WASM ONLINE";
      
      setInterval(() => {
        if (!authorizedUser) processFrame();
      }, 80);
    } catch (e) {
      status = "ERROR DE CARGA";
      console.error(e);
    }
  }

  // ... (Funciones processFrame y captureThumb se mantienen igual) ...

  async function processFrame() {
    if (!videoElement || videoElement.readyState < 2) return;
    const detection = await faceapi.detectSingleFace(videoElement, new faceapi.TinyFaceDetectorOptions()).withFaceLandmarks();
    if (!detection) { eyesLocked = false; return; }

    const landmarks = detection.landmarks;
    const box = detection.detection.box;
    
    faceBox = {
      x: (box.x / videoElement.videoWidth) * 100,
      y: (box.y / videoElement.videoHeight) * 100,
      w: (box.width / videoElement.videoWidth) * 100,
      h: (box.height / videoElement.videoHeight) * 100
    };

    const lEye = landmarks.getLeftEye();
    const rEye = landmarks.getRightEye();

    leftCircle = {
      x: (lEye.reduce((a, b) => a + b.x, 0) / 6 / videoElement.videoWidth) * 100,
      y: (lEye.reduce((a, b) => a + b.y, 0) / 6 / videoElement.videoHeight) * 100,
      active: true
    };

    rightCircle = {
      x: (rEye.reduce((a, b) => a + b.x, 0) / 6 / videoElement.videoWidth) * 100,
      y: (rEye.reduce((a, b) => a + b.y, 0) / 6 / videoElement.videoHeight) * 100,
      active: true
    };

    eyesLocked = true;
    points = landmarks.positions.map(p => ({
      x: (p.x / videoElement.videoWidth) * 100,
      y: (p.y / videoElement.videoHeight) * 100
    }));

    const ear = (() => {
        const d = (p1, p2) => Math.sqrt(Math.pow(p1.x-p2.x,2) + Math.pow(p1.y-p2.y,2));
        const eyeEar = (p) => (d(p[1],p[5]) + d(p[2],p[4])) / (2 * d(p[0],p[3]));
        return (eyeEar(lEye) + eyeEar(rEye)) / 2;
    })();

    if (lastEar > 0.25 && ear < 0.18) {
      blinkCount++;
      if (blinkCount >= 2) handleBiometricCheck();
    }
    lastEar = ear;
  }

  onMount(() => {
    navigator.mediaDevices.getUserMedia({ video: { width: 1280, height: 720 } })
      .then(s => { 
        videoElement.srcObject = s; 
        loadIA(); 
      });
  });

  async function captureThumb(): Promise<string> {
    const canvas = document.createElement('canvas');
    canvas.width = 160; canvas.height = 160;
    const ctx = canvas.getContext('2d');
    if (videoElement) ctx.drawImage(videoElement, 0, 0, 160, 160);
    return canvas.toDataURL('image/jpeg', 0.7);
  }
</script>

<div class="main-container">
  <div class="scanner-frame" style="--zoom: {eyesLocked ? 1.3 : 1}">
    <video bind:this={videoElement} autoplay playsinline muted></video>
    
    <div class="dynamic-geometry">
        <div class="tracker" class:active={leftCircle.active} class:locked={eyesLocked}
             style:left="{leftCircle.x}%" style:top="{leftCircle.y}%"></div>
        <div class="tracker" class:active={rightCircle.active} class:locked={eyesLocked}
             style:left="{rightCircle.x}%" style:top="{rightCircle.y}%"></div>
        
        {#if eyesLocked && authorizedUser}
            <div class="floating-tag" style:left="{faceBox.x}%" style:top="{faceBox.y}%">
                <div class="tag-connector"></div>
                <div class="tag-body">
                    <img src={authorizedUser.photo} alt="bio-capture" />
                    <div class="tag-details">
                        <div class="tag-label">MATCH CONFIRMED</div>
                        <div class="tag-name">{authorizedUser.name}</div>
                        <div class="tag-hash">{authorizedUser.hash}</div>
                    </div>
                </div>
            </div>
        {/if}
    </div>

    <svg class="biometric-mesh" viewBox="0 0 100 100" preserveAspectRatio="none">
        {#if eyesLocked && !authorizedUser}
            {#each points as p}
                <circle cx="{p.x}" cy="{p.y}" r="0.12" fill="#00ffff" />
            {/each}
        {/if}
    </svg>

    <div class="hud">
      <div class="status-box" class:authorized={authorizedUser}>
        <span class="glitch-text">{status}</span>
      </div>
      
      {#if !authorizedUser}
        <div class="blink-progress">
            <div class="progress-segment" class:fill={blinkCount >= 1}></div>
            <div class="progress-segment" class:fill={blinkCount >= 2}></div>
            <span class="progress-label">LIVENESS TEST</span>
        </div>
      {:else}
        <button class="action-btn" onclick={() => { authorizedUser = null; blinkCount = 0; }}>
            RE-SCAN TARGET
        </button>
      {/if}
    </div>
  </div>
</div>

<style>
  :global(body) { background: #020202; margin: 0; font-family: 'Share Tech Mono', monospace; overflow: hidden; }
  
  .main-container { width: 100vw; height: 100vh; display: flex; align-items: center; justify-content: center; }
  
  .scanner-frame { 
    position: relative; width: 850px; height: 500px; 
    border: 1px solid #111; overflow: hidden; background: #000;
    box-shadow: 0 0 40px rgba(0, 255, 255, 0.05);
  }
  
  video { 
    width: 100%; height: 100%; object-fit: cover; 
    transform: scaleX(-1) scale(var(--zoom)); 
    transition: transform 1.5s cubic-bezier(0.19, 1, 0.22, 1); 
    filter: brightness(0.6) contrast(1.3) grayscale(0.2) sepia(0.2); 
  }

  .dynamic-geometry { 
    position: absolute; inset: 0; pointer-events: none; z-index: 10; 
    transform: scaleX(-1) scale(var(--zoom)); transition: 1.5s; 
  }

  /* ETIQUETA FLOTANTE (ESTILO CIBERPUNK) */
  .floating-tag { 
    position: absolute; transform: translate(-105%, -40%) scaleX(-1); 
    z-index: 50; animation: tagAppear 0.4s ease-out; 
  }
  
  .tag-body { 
    background: rgba(0, 15, 15, 0.85); border-left: 3px solid #0ff; 
    padding: 10px; display: flex; gap: 12px; backdrop-filter: blur(8px);
    box-shadow: 10px 10px 20px rgba(0,0,0,0.5);
  }

  .tag-body img { width: 60px; height: 60px; border: 1px solid rgba(0,255,255,0.3); object-fit: cover; }
  
  .tag-details { display: flex; flex-direction: column; justify-content: center; }
  
  .tag-label { font-size: 8px; color: #0ff; opacity: 0.6; letter-spacing: 1px; }
  .tag-name { font-size: 16px; color: #0ff; text-shadow: 0 0 5px #0ff; }
  .tag-hash { font-size: 10px; color: #aaa; margin-top: 2px; }

  .tag-connector { 
    position: absolute; right: -30px; top: 50%; width: 30px; height: 1px; 
    background: linear-gradient(to right, #0ff, transparent); 
  }

  .tracker { 
    position: absolute; width: 30px; height: 30px; 
    border: 1px solid rgba(0, 255, 255, 0.2); transform: translate(-50%, -50%); 
  }
  .tracker.locked { border-color: #0ff; border-width: 2px; box-shadow: 0 0 15px #0ff; }

  .biometric-mesh { 
    position: absolute; inset: 0; z-index: 5; pointer-events: none; 
    transform: scaleX(-1) scale(var(--zoom)); transition: 1.5s; 
  }

  .hud { 
    position: absolute; bottom: 0; width: 100%; 
    padding: 30px; display: flex; flex-direction: column; align-items: center;
    background: linear-gradient(transparent, rgba(0,0,0,0.8));
    z-index: 100;
  }

  .status-box { margin-bottom: 15px; color: #0ff; letter-spacing: 5px; font-size: 14px; }
  .status-box.authorized { color: #0f0; text-shadow: 0 0 10px #0f0; }

  .blink-progress { display: flex; align-items: center; gap: 10px; }
  .progress-segment { width: 40px; height: 4px; background: rgba(0,255,255,0.1); border: 1px solid #0ff; }
  .progress-segment.fill { background: #0ff; box-shadow: 0 0 10px #0ff; }
  .progress-label { font-size: 9px; color: #0ff; margin-left: 10px; }

  .action-btn { 
    background: #0ff; color: #000; border: none; padding: 8px 25px; 
    font-family: inherit; font-size: 11px; cursor: pointer; font-weight: bold;
  }

  @keyframes tagAppear {
    from { opacity: 0; transform: translate(-105%, -20%) scaleX(-1); }
    to { opacity: 1; transform: translate(-105%, -40%) scaleX(-1); }
  }
</style>