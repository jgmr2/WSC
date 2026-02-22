<script lang="ts">
  import { onMount } from 'svelte';

  // --- ESTADOS CON RUNES (SVELTE 5) ---
  let videoElement = $state<HTMLVideoElement | null>(null);
  let status = $state("SISTEMA INICIANDO...");
  let points = $state<{x: number, y: number}[]>([]); 
  let eyesLocked = $state(false);
  let blinkCount = $state(0);
  let authorizedUser = $state<any>(null);
  let faceBox = $state({ x: 0, y: 0, w: 0, h: 0 }); 
  let leftCircle = $state({ x: 38, y: 45, active: false });
  let rightCircle = $state({ x: 62, y: 45, active: false });

  // --- INTEGRACIÓN MEDIA PIPE Y WASM ---
  let faceLandmarker: any;
  let wasmEngine: any = null;
  let isBlinking = false; // Estado local para control de parpadeo
  
  const wasmPath = '/camara.js';
  const DB_KEY = "BIOMETRIC_LOCAL_DB";

  // Promesa para cargar tu motor C++
  const motorPromise = import(/* @vite-ignore */ wasmPath)
    .then(m => m.default({ locateFile: (p: string) => `/${p}` }));

  onMount(async () => {
    try {
      // 1. Carga dinámica de MediaPipe para evitar errores de SSR en el Build
      const mpTasks = await import("@mediapipe/tasks-vision");
      const { FaceLandmarker, FilesetResolver } = mpTasks;

      // 2. Inicializar WASM de C++ y Resolvers en paralelo
      const [engine, filesetResolver] = await Promise.all([
        motorPromise,
        FilesetResolver.forVisionTasks("https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@latest/wasm")
      ]);

      wasmEngine = engine;

      // 3. Configurar Landmarker
      faceLandmarker = await FaceLandmarker.createFromOptions(filesetResolver, {
        baseOptions: {
          modelAssetPath: `https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task`,
          delegate: "GPU"
        },
        outputFaceBlendshapes: true,
        runningMode: "VIDEO",
        numFaces: 1
      });

      status = "MOTOR BIOMÉTRICO ONLINE";
      
      // 4. Iniciar cámara SOLO si el elemento ya existe
      if (videoElement) {
        await setupCamera();
      }
    } catch (e) {
      status = "ERROR DE INICIALIZACIÓN";
      console.error(e);
    }
  });

  async function setupCamera() {
    if (!videoElement) return;

    try {
      const stream = await navigator.mediaDevices.getUserMedia({ 
          video: { width: 1280, height: 720 } 
      });
      
      videoElement.srcObject = stream;
      
      // Esperar a que el video esté listo para procesar
      videoElement.onloadedmetadata = () => {
        videoElement?.play();
        predict();
      };
    } catch (err) {
      status = "ERROR: ACCESO A CÁMARA DENEGADO";
    }
  }

  async function predict() {
    if (!videoElement || !faceLandmarker || videoElement.paused) return;

    const startTimeMs = performance.now();
    const results = faceLandmarker.detectForVideo(videoElement, startTimeMs);

    if (results.faceLandmarks && results.faceLandmarks.length > 0) {
      const landmarks = results.faceLandmarks[0];
      const blendShapes = results.faceBlendshapes[0]?.categories || [];

      processFaceData(landmarks, blendShapes);
      eyesLocked = true;
    } else {
      eyesLocked = false;
      leftCircle.active = false;
      rightCircle.active = false;
    }

    requestAnimationFrame(predict);
  }

  function processFaceData(landmarks: any[], blendShapes: any[]) {
    // MediaPipe 478 puntos normalizados a %
    points = landmarks.map(p => ({ x: p.x * 100, y: p.y * 100 }));

    // Bounding Box para el UI
    const xs = landmarks.map(p => p.x);
    const ys = landmarks.map(p => p.y);
    const minX = Math.min(...xs);
    const maxX = Math.max(...xs);
    const minY = Math.min(...ys);
    const maxY = Math.max(...ys);

    faceBox = {
      x: minX * 100,
      y: minY * 100,
      w: (maxX - minX) * 100,
      h: (maxY - minY) * 100
    };

    // Tracking de Iris (MediaPipe indices 468 y 473)
    leftCircle = { x: landmarks[468].x * 100, y: landmarks[468].y * 100, active: true };
    rightCircle = { x: landmarks[473].x * 100, y: landmarks[473].y * 100, active: true };

    // Detección de parpadeo vía Blendshapes
    const blinkL = blendShapes.find(s => s.categoryName === "eyeBlinkLeft")?.score || 0;
    const blinkR = blendShapes.find(s => s.categoryName === "eyeBlinkRight")?.score || 0;
    
    if (blinkL > 0.5 && blinkR > 0.5) {
        if (!isBlinking) {
            isBlinking = true;
            blinkCount++;
            if (blinkCount >= 2 && !authorizedUser) handleBiometricCheck();
        }
    } else {
        isBlinking = false;
    }
  }

  function getWasmHash(currentPoints: any[]) {
    if (!wasmEngine || currentPoints.length === 0) return null;
    const flatCoords = new Float32Array(currentPoints.flatMap(p => [p.x, p.y]));
    const ptr = wasmEngine._malloc(flatCoords.length * 4);
    wasmEngine.HEAPF32.set(flatCoords, ptr >> 2);
    const hash = wasmEngine.generate_geometric_hash(ptr, currentPoints.length);
    wasmEngine._free(ptr);
    return hash;
  }

  async function handleBiometricCheck() {
    if (!wasmEngine) return;
    status = "VALIDANDO GEOMETRÍA...";
    
    const currentHash = getWasmHash(points);
    const db = JSON.parse(localStorage.getItem(DB_KEY) || "[]");
    
    const match = db.find((user: any) => wasmEngine.get_similarity(currentHash, user.hash) > 0.85);

    if (match) {
      authorizedUser = match;
      status = "IDENTIDAD CONFIRMADA";
    } else {
      const thumb = await captureThumb();
      const newUser = {
        name: `SUJETO_${db.length + 101}`,
        hash: currentHash,
        photo: thumb,
        timestamp: new Date().toLocaleString()
      };
      db.push(newUser);
      localStorage.setItem(DB_KEY, JSON.stringify(db));
      authorizedUser = newUser;
      status = "NUEVO REGISTRO CREADO";
    }
  }

  async function captureThumb(): Promise<string> {
    const canvas = document.createElement('canvas');
    canvas.width = 160; canvas.height = 160;
    const ctx = canvas.getContext('2d');
    if (videoElement && ctx) ctx.drawImage(videoElement, 0, 0, 160, 160);
    return canvas.toDataURL('image/jpeg', 0.7);
  }

  function getCorrectedCoords(p: {x: number, y: number}) {
  if (!videoElement) return { x: 0, y: 0 };

  const displayW = videoElement.offsetWidth;
  const displayH = videoElement.offsetHeight;
  const videoW = videoElement.videoWidth;
  const videoH = videoElement.videoHeight;

  const videoAspect = videoW / videoH;
  const displayAspect = displayW / displayH;

  let renderW, renderH;
  let offsetX = 0;
  let offsetY = 0;

  // Lógica exacta de object-fit: cover
  if (videoAspect > displayAspect) {
    // El video es más ancho: se ajusta al alto y se corta de los lados
    renderH = displayH;
    renderW = displayH * videoAspect;
    offsetX = (renderW - displayW) / 2;
  } else {
    // El video es más alto: se ajusta al ancho y se corta arriba/abajo
    renderW = displayW;
    renderH = displayW / videoAspect;
    offsetY = (renderH - displayH) / 2;
  }

  // Convertimos la coordenada normalizada (0-1) a píxeles de renderizado
  // (1 - p.x) para compensar el scaleX(-1) del CSS
  const xPx = (1 - p.x) * renderW - offsetX;
  const yPx = p.y * renderH - offsetY;

  // Devolvemos en porcentaje respecto al contenedor visible
  return { 
    x: (xPx / displayW) * 100, 
    y: (yPx / displayH) * 100 
  };
}
</script>
<div class="main-container">
  <div class="scanner-frame" style="--zoom: {eyesLocked ? 1.2 : 1}">
    
    <video bind:this={videoElement} autoplay playsinline muted></video>
    
    <div class="dynamic-geometry">
        <div class="tracker" class:active={leftCircle.active} class:locked={eyesLocked}
             style:left="{leftCircle.x}%" style:top="{leftCircle.y}%"></div>
        <div class="tracker" class:active={rightCircle.active} class:locked={eyesLocked}
             style:left="{rightCircle.x}%" style:top="{rightCircle.y}%"></div>
        
        {#if eyesLocked && authorizedUser}
            <div class="floating-tag" style:left="{faceBox.x}%" style:top="{faceBox.y}%">
                <div class="tag-body">
                    <img src={authorizedUser.photo} alt="bio-capture" />
                    <div class="tag-details">
                        <div class="tag-label">MATCH CONFIRMED</div>
                        <div class="tag-name">{authorizedUser.name}</div>
                        <div class="tag-hash">{authorizedUser.hash.substring(0, 15)}...</div>
                    </div>
                </div>
            </div>
        {/if}
    </div>

    <svg class="biometric-mesh" viewBox="0 0 100 100" preserveAspectRatio="none">
        {#if eyesLocked && !authorizedUser}
            {#each points as p}
                <circle cx="{p.x}" cy="{p.y}" r="0.15" fill="#00ffff" opacity="0.5" />
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
            <span class="progress-label">LIVENESS TEST (BLINK)</span>
        </div>
      {:else}
        <button class="action-btn" onclick={() => { authorizedUser = null; blinkCount = 0; }}>
            RESET SCANNER
        </button>
      {/if}
    </div>
  </div>
</div>

<style>
  :global(body) { background: #020202; margin: 0; font-family: 'Share Tech Mono', monospace; overflow: hidden; }
  
  .main-container { width: 100vw; height: 100vh; display: flex; align-items: center; justify-content: center; }

  .scanner-frame { 
    position: relative; 
    width: 850px; 
    /* Forzamos que el contenedor tenga la misma proporción que la cámara */
    aspect-ratio: 16 / 9; 
    border: 1px solid rgba(0, 255, 255, 0.2); 
    overflow: hidden; 
    background: #000; 
  }

  video { 
    position: absolute;
    top: 0;
    left: 0;
    width: 100%; 
    height: 100%; 
    /* Cambiamos cover por fill para que coincida con el sistema de puntos 0-100% */
    object-fit: fill; 
    transform: scaleX(-1) scale(var(--zoom)); 
    transition: transform 1.5s cubic-bezier(0.19, 1, 0.22, 1); 
    filter: brightness(0.6) contrast(1.2); 
  }

  /* Todas las capas de overlay comparten el mismo tamaño y transformación que el video */
  .dynamic-geometry, .biometric-mesh { 
    position: absolute; 
    inset: 0; 
    width: 100%;
    height: 100%;
    pointer-events: none; 
    z-index: 10; 
    /* IMPORTANTE: Mismo espejo y zoom que el video */
    transform: scaleX(-1) scale(var(--zoom)); 
    transition: transform 1.5s cubic-bezier(0.19, 1, 0.22, 1); 
  }

  /* El tag debe invertir el espejo del padre para que el texto sea legible */
  .floating-tag { 
    position: absolute; 
    transform: translate(-105%, -40%) scaleX(-1); 
    z-index: 50; 
  }

  .tag-body { 
    background: rgba(0, 15, 15, 0.85); 
    border-left: 3px solid #0ff; 
    padding: 10px; 
    display: flex; 
    gap: 12px; 
    backdrop-filter: blur(8px); 
    min-width: 200px;
  }

  .tag-body img { width: 60px; height: 60px; border: 1px solid rgba(0,255,255,0.3); object-fit: cover; }
  .tag-details { color: #0ff; }
  .tag-name { font-size: 14px; font-weight: bold; }
  .tag-label { font-size: 10px; opacity: 0.7; }
  .tag-hash { font-size: 8px; margin-top: 4px; opacity: 0.5; }

  .tracker { 
    position: absolute; 
    width: 20px; 
    height: 20px; 
    border: 1px solid rgba(0, 255, 255, 0.3); 
    transform: translate(-50%, -50%); 
    border-radius: 50%; 
  }
  
  .tracker.locked { border-color: #0ff; box-shadow: 0 0 10px #0ff; }

  .biometric-mesh { z-index: 5; }

  .hud { 
    position: absolute; 
    bottom: 20px; 
    width: 100%; 
    display: flex; 
    flex-direction: column; 
    align-items: center; 
    z-index: 100; 
  }

  .status-box { color: #0ff; letter-spacing: 3px; font-size: 12px; margin-bottom: 10px; text-transform: uppercase; }
  .blink-progress { display: flex; gap: 8px; align-items: center; }
  .progress-segment { width: 30px; height: 4px; border: 1px solid rgba(0,255,255,0.3); }
  .progress-segment.fill { background: #0ff; box-shadow: 0 0 10px #0ff; }
  .progress-label { color: #0ff; font-size: 9px; margin-left: 10px; }

  .action-btn { 
    background: #0ff; 
    color: #000; 
    border: none; 
    padding: 8px 20px; 
    cursor: pointer; 
    font-family: inherit; 
    font-weight: bold;
    text-transform: uppercase;
  }
</style>