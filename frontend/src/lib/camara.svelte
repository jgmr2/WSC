<script lang="ts">
  import { onMount } from 'svelte';

  // --- ESTADOS ---
  let videoElement = $state<HTMLVideoElement | null>(null);
  let status = $state("SISTEMA LISTO");
  let points = $state<{x: number, y: number}[]>([]); 
  let eyesLocked = $state(false);
  let blinkCount = $state(0);
  let authorizedUser = $state<any>(null);
  let faceBox = $state({ x: 0, y: 0, w: 0, h: 0 }); 
  let leftCircle = $state({ x: 38, y: 45, active: false });
  let rightCircle = $state({ x: 62, y: 45, active: false });

  // --- LÓGICA DE CONTROL Y ESTADÍSTICA ---
  let isProcessing = $state(false);
  let lastCheckTime = 0;
  const CHECK_COOLDOWN = 3000; // Espera 3s entre escaneos si falla
  const MATCH_THRESHOLD = 0.99; // 94% de similitud para validar
  const NEW_USER_THRESHOLD = 0.82; // Menos de 82% se considera extraño (nuevo registro)

  let faceLandmarker: any;
  let wasmEngine: any = null;
  let isBlinking = false; 
  let rawLandmarks: any[] = []; // Guardamos los datos puros para el vector
  
  const wasmPath = '/camara.js';
  const mediapipePath = '/mediapipe';
  const DB_KEY = "BIOMETRIC_LOCAL_DB";

  const motorPromise = import(/* @vite-ignore */ wasmPath)
    .then(m => m.default({ locateFile: (p: string) => `/${p}` }));

  onMount(() => {
    const initializeAI = async () => {
      try {
        status = "CARGANDO MÓDULOS IA...";
        const mpTasks = await import("@mediapipe/tasks-vision");
        const { FaceLandmarker, FilesetResolver } = mpTasks;

        const [engine, filesetResolver] = await Promise.all([
          motorPromise,
          FilesetResolver.forVisionTasks(mediapipePath)
        ]);

        wasmEngine = engine;

        faceLandmarker = await FaceLandmarker.createFromOptions(filesetResolver, {
          baseOptions: {
            modelAssetPath: `${mediapipePath}/face_landmarker.task`,
            delegate: "GPU"
          },
          outputFaceBlendshapes: true,
          runningMode: "VIDEO",
          numFaces: 1
        });

        status = "SISTEMA ONLINE";
        setTimeout(() => { if (videoElement) setupCamera(); }, 500);
      } catch (e) {
        status = "ERROR DE SISTEMA";
        console.error(e);
      }
    };
    initializeAI();
  });

  // --- MATEMÁTICAS BIOMÉTRICAS ---

  /** * Normaliza los puntos: Invariante a posición (resta nariz) y escala (divide por dist. ojos)
   */
  function getFeatureVector(landmarks: any[]) {
    const anchor = landmarks[1]; // Punta de la nariz
    const leftEye = landmarks[33];
    const rightEye = landmarks[263];
    
    // Escala basada en distancia euclidiana entre ojos
    const scale = Math.sqrt(
      Math.pow(rightEye.x - leftEye.x, 2) + Math.pow(rightEye.y - leftEye.y, 2)
    ) || 1;

    const vector = new Float32Array(landmarks.length * 3);
    for (let i = 0; i < landmarks.length; i++) {
        vector[i * 3]     = (landmarks[i].x - anchor.x) / scale;
        vector[i * 3 + 1] = (landmarks[i].y - anchor.y) / scale;
        vector[i * 3 + 2] = (landmarks[i].z - anchor.z) / scale;
    }
    return vector;
  }

  /**
   * Calcula la similitud del coseno entre dos vectores
   */
  function calculateCosineSimilarity(vecA: Float32Array, vecB: Float32Array) {
    let dotProduct = 0, normA = 0, normB = 0;
    for (let i = 0; i < vecA.length; i++) {
        dotProduct += vecA[i] * vecB[i];
        normA += vecA[i] * vecA[i];
        normB += vecB[i] * vecB[i];
    }
    return dotProduct / (Math.sqrt(normA) * Math.sqrt(normB));
  }

  // --- FLUJO DE CÁMARA ---

  async function setupCamera() {
    if (!videoElement) return;
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ 
          video: { width: 1280, height: 720 } 
      });
      videoElement.srcObject = stream;
      videoElement.onloadedmetadata = () => {
        videoElement?.play();
        requestAnimationFrame(predict);
      };
    } catch (err) {
      status = "CÁMARA NO DISPONIBLE";
    }
  }

  async function predict() {
    if (!videoElement || !faceLandmarker || videoElement.paused) return;
    const results = faceLandmarker.detectForVideo(videoElement, performance.now());

    if (results.faceLandmarks?.length > 0) {
      rawLandmarks = results.faceLandmarks[0];
      processFaceData(rawLandmarks, results.faceBlendshapes[0]?.categories || []);
      eyesLocked = true;
    } else {
      eyesLocked = false;
      leftCircle.active = false;
      rightCircle.active = false;
    }
    requestAnimationFrame(predict);
  }

  function processFaceData(landmarks: any[], blendShapes: any[]) {
    points = landmarks.map(p => ({ x: p.x * 100, y: p.y * 100 }));

    if (!authorizedUser) {
        const xs = landmarks.map(p => p.x);
        const ys = landmarks.map(p => p.y);
        faceBox = {
          x: Math.min(...xs) * 100, y: Math.min(...ys) * 100,
          w: (Math.max(...xs) - Math.min(...xs)) * 100, 
          h: (Math.max(...ys) - Math.min(...ys)) * 100
        };
    }

    leftCircle = { x: landmarks[468].x * 100, y: landmarks[468].y * 100, active: true };
    rightCircle = { x: landmarks[473].x * 100, y: landmarks[473].y * 100, active: true };

    const blinkL = blendShapes.find(s => s.categoryName === "eyeBlinkLeft")?.score || 0;
    const blinkR = blendShapes.find(s => s.categoryName === "eyeBlinkRight")?.score || 0;
    
    if (blinkL > 0.5 && blinkR > 0.5) {
        if (!isBlinking) {
            isBlinking = true;
            blinkCount++;
            // Solo disparamos si no estamos procesando y pasó el cooldown
            if (blinkCount >= 2 && !authorizedUser) handleBiometricCheck();
        }
    } else {
        isBlinking = false;
    }
  }

  // --- LÓGICA DE VALIDACIÓN MEJORADA ---

  async function handleBiometricCheck() {
    const now = Date.now();
    if (isProcessing || (now - lastCheckTime < CHECK_COOLDOWN)) return;
    
    isProcessing = true;
    status = "VERIFICANDO IDENTIDAD...";
    
    try {
        const currentVector = getFeatureVector(rawLandmarks);
        const db = JSON.parse(localStorage.getItem(DB_KEY) || "[]");
        
        let bestMatch = null;
        let highestScore = 0;

        // Búsqueda estadística en la "DB" local
        for (const user of db) {
            const savedVector = new Float32Array(user.vector);
            const score = calculateCosineSimilarity(currentVector, savedVector);
            if (score > highestScore) {
                highestScore = score;
                bestMatch = user;
            }
        }

        if (highestScore > MATCH_THRESHOLD) {
            authorizedUser = bestMatch;
            status = `ACCESO: ${(highestScore * 100).toFixed(1)}%`;
        } else if (highestScore < NEW_USER_THRESHOLD) {
            // Si nadie se parece lo suficiente, registramos
            const thumb = await captureThumb();
            const newUser = {
                id: crypto.randomUUID(),
                name: `ID_${Math.random().toString(36).substr(2, 5).toUpperCase()}`,
                vector: Array.from(currentVector), // JSON no soporta Float32Array directamente
                photo: thumb,
                timestamp: new Date().toISOString()
            };
            db.push(newUser);
            localStorage.setItem(DB_KEY, JSON.stringify(db));
            authorizedUser = newUser;
            status = "REGISTRO COMPLETADO";
        } else {
            // Caso donde el score es medio (0.83 - 0.93): ruido o mala pose
            status = "REINTENTE (POSICIÓN INESTABLE)";
        }
    } catch (e) {
        status = "ERROR DE PROCESAMIENTO";
    } finally {
        isProcessing = false;
        lastCheckTime = Date.now();
    }
  }

  async function captureThumb(): Promise<string> {
    const canvas = document.createElement('canvas');
    canvas.width = 160; canvas.height = 160;
    const ctx = canvas.getContext('2d');
    if (videoElement && ctx) {
        // Recortamos el área de la cara para el thumb si es posible, 
        // o simplemente capturamos el centro del video
        ctx.drawImage(videoElement, videoElement.videoWidth/2 - 360, 0, 720, 720, 0, 0, 160, 160);
    }
    return canvas.toDataURL('image/jpeg', 0.7);
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
                        <div class="tag-hash">
                          ID: {authorizedUser.id?.substring(0, 8) || '00000000'}
                        </div>
                    </div>
                </div>
            </div>
        {/if}
    </div>

    <svg class="biometric-mesh" viewBox="0 0 100 100" preserveAspectRatio="none">
        {#if eyesLocked && !authorizedUser}
            {#each points as p}
                <circle cx="{p.x}" cy="{p.y}" r="0.18" 
                        fill={isProcessing ? "#ff00ff" : "#00ffff"} 
                        opacity={isProcessing ? "0.8" : "0.4"} />
            {/each}
        {/if}
    </svg>

    <div class="hud">
      <div class="status-box" class:authorized={authorizedUser} class:scanning={isProcessing}>
        <span class="glitch-text">{status}</span>
      </div>
      
      {#if !authorizedUser}
        <div class="blink-progress" class:disabled={isProcessing}>
            <div class="progress-segment" class:fill={blinkCount >= 1}></div>
            <div class="progress-segment" class:fill={blinkCount >= 2}></div>
            <span class="progress-label">
                {isProcessing ? 'CALCULATING VECTOR...' : 'LIVENESS TEST (BLINK)'}
            </span>
        </div>
      {:else}
        <button class="action-btn" onclick={() => { authorizedUser = null; blinkCount = 0; status = "SISTEMA ONLINE"; }}>
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