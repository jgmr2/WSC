<script lang="ts">
  import { onMount } from 'svelte';

  // --- ESTADOS DE SISTEMA (Mantenidos) ---
  let videoElement = $state<HTMLVideoElement | null>(null);
  let status = $state("SISTEMA LISTO");
  let points = $state<{x: number, y: number, z: number}[]>([]); 
  let eyesLocked = $state(false);
  let blinkCount = $state(0);
  let authorizedUser = $state<any>(null);
  let faceBox = $state({ x: 0, y: 0, w: 0, h: 0 }); 
  let isBlinking = $state(false); 
  let leftCircle = $state({ x: 0, y: 0, active: false });
  let rightCircle = $state({ x: 0, y: 0, active: false });

  // --- LÓGICA DE REGISTRO MULTI-POSE (Mejorada) ---
  let isRegistering = $state(false);
  let regProgress = $state(0);
  let galleryVectors = $state<Record<string, number[]>>({}); // Galería de promedios
  let poseBuffer: Float32Array[] = []; 
  const POSE_SAMPLES = 5; // Frames por ángulo

  let anglesDone = $state({ left: false, right: false, up: false, down: false, center: false });
  let isStabilized = $state(false);
  let stabilityStartTime = $state<number | null>(null);

  // --- LÓGICA DE LOGIN (Matemáticas Optimizadas) ---
  let isProcessing = $state(false);
  let frameBuffer: Float32Array[] = []; 
  const SAMPLES_REQUIRED = 2; 
  const MATCH_THRESHOLD = 0.99; // Umbral realista para entornos variables
  const NEW_USER_THRESHOLD = 0.80;
  const DB_KEY = "BIOMETRIC_LOCAL_DB_V2";

  let faceLandmarker: any;
  let rawLandmarks: any[] = []; 
  const mediapipePath = '/mediapipe';

  // --- DERIVADOS (UI) ---
  let nextGoal = $derived(() => {
    if (!isRegistering) return null;
    if (!anglesDone.left) return { icon: '←' };
    if (!anglesDone.right) return { icon: '→' };
    if (!anglesDone.up) return { icon: '↑' };
    if (!anglesDone.down) return { icon: '↓' };
    if (!isStabilized) return { icon: '⌛' };
    if (!anglesDone.center) return { icon: '👁️' };
    return null;
  });

  onMount(() => {
    const initializeAI = async () => {
      try {
        status = "CARGANDO IA...";
        const mpTasks = await import("@mediapipe/tasks-vision");
        const { FaceLandmarker, FilesetResolver } = mpTasks;
        const filesetResolver = await FilesetResolver.forVisionTasks(mediapipePath);

        faceLandmarker = await FaceLandmarker.createFromOptions(filesetResolver, {
          baseOptions: { modelAssetPath: `${mediapipePath}/face_landmarker.task`, delegate: "GPU" },
          outputFaceBlendshapes: true,
          runningMode: "VIDEO",
          numFaces: 1
        });

        status = "SISTEMA ONLINE";
        setupCamera();
      } catch (e) { status = "ERROR DE SISTEMA"; }
    };
    initializeAI();
  });

  // --- MATEMÁTICAS AVANZADAS ---
function getFeatureVector(landmarks: any[]) {
    const anchor = landmarks[1]; // Nariz (punto estable)
    
    // Calculamos una escala basada en el volumen del rostro, no solo una línea
    const top = landmarks[10], bottom = landmarks[152];
    const left = landmarks[234], right = landmarks[454];
    
    const height = Math.sqrt(Math.pow(top.x - bottom.x, 2) + Math.pow(top.y - bottom.y, 2));
    const width = Math.sqrt(Math.pow(left.x - right.x, 2) + Math.pow(left.y - right.y, 2));
    const scale = (height + width) / 2 || 1;

    const vector = new Float32Array(landmarks.length * 3);
    for (let i = 0; i < landmarks.length; i++) {
        // Normalizamos coordenadas y aplicamos un factor de suavizado al eje Z
        vector[i * 3] = (landmarks[i].x - anchor.x) / scale;
        vector[i * 3 + 1] = (landmarks[i].y - anchor.y) / scale;
        vector[i * 3 + 2] = (landmarks[i].z - anchor.z) / (scale * 0.5); // Reducimos ruido Z
    }
    return vector;
  }
  function getFaceOrientation(landmarks: any[]) {
    const left = landmarks[234], right = landmarks[454], top = landmarks[10], bottom = landmarks[152];
    return { yaw: right.z - left.z, pitch: top.z - bottom.z };
  }

function calculateCosineSimilarity(vecA: Float32Array, vecB: Float32Array) {
    let dotProduct = 0, normA = 0, normB = 0;
    for (let i = 0; i < vecA.length; i++) {
        dotProduct += vecA[i] * vecB[i];
        normA += vecA[i] * vecA[i];
        normB += vecB[i] * vecB[i];
    }
    const similarity = dotProduct / (Math.sqrt(normA) * Math.sqrt(normB));
    
    // Si la similitud es negativa o extremadamente baja, retornamos 0
    return Math.max(0, similarity);
  }

  function averageVectors(vectors: Float32Array[]): number[] {
    const size = vectors[0].length;
    const avg = new Float32Array(size);
    for (let i = 0; i < size; i++) {
      let sum = 0;
      for (let v of vectors) sum += v[i];
      avg[i] = sum / vectors.length;
    }
    return Array.from(avg);
  }

  // --- LÓGICA DE NEGOCIO ---
async function handleBiometricCheck() {
    if (isProcessing) return;
    
    const currentVector = getFeatureVector(rawLandmarks);
    if (frameBuffer.length < SAMPLES_REQUIRED) frameBuffer.push(currentVector);
    
    status = `PROCESANDO IDENTIDAD...`;

    if (frameBuffer.length >= SAMPLES_REQUIRED) {
        isProcessing = true;
        try {
            const consensusCurrent = new Float32Array(averageVectors(frameBuffer));
            const db = JSON.parse(localStorage.getItem(DB_KEY) || "[]");
            let bestMatch = null, highestScore = 0;

            const { yaw, pitch } = getFaceOrientation(rawLandmarks);
            const isFrontal = Math.abs(yaw) < 0.1 && Math.abs(pitch) < 0.1;

            for (const user of db) {
                const scoreCenter = calculateCosineSimilarity(consensusCurrent, new Float32Array(user.gallery.center));
                let finalScore = 0;

                if (isFrontal) {
                    finalScore = scoreCenter;
                } else {
                    let bestAngleScore = 0;
                    for (const angleKey in user.gallery) {
                        if (angleKey === 'center') continue;
                        const s = calculateCosineSimilarity(consensusCurrent, new Float32Array(user.gallery[angleKey]));
                        if (s > bestAngleScore) bestAngleScore = s;
                    }
                    finalScore = (scoreCenter * 0.4) + (bestAngleScore * 0.6);
                }

                if (finalScore > highestScore) {
                  highestScore = finalScore;
                  bestMatch = user;
                }
            }

            // --- LÓGICA DE DECISIÓN CON PRECISIÓN DE 5 DECIMALES ---
            if (highestScore >= MATCH_THRESHOLD) {
                authorizedUser = bestMatch;
                // Mostramos el nombre y el score con precisión extrema (ej: 0.85231)
                status = `ACCESO: ${bestMatch.name} (${highestScore.toFixed(5)})`;
            } else if (highestScore < NEW_USER_THRESHOLD) {
                status = "USUARIO NO RECONOCIDO";
                startPoseRegistration();
            } else {
                // Si falla, también mostramos la precisión para saber qué tan cerca estuvimos
                status = `REINTENTE - SCORE: ${highestScore.toFixed(5)}`;
            }

            frameBuffer = [];
            blinkCount = 0;
            
        } catch (error) {
            status = "ERROR DE CÁLCULO";
            console.error(error);
        } finally { 
            isProcessing = false; 
        }
    }
  }

  function handleRegistrationPose(landmarks: any[]) {
    const { yaw, pitch } = getFaceOrientation(landmarks);
    const vec = getFeatureVector(landmarks);

    const captureAngle = (key: string) => {
      poseBuffer.push(vec);
      status = `CAPTURANDO ${key.toUpperCase()}: ${poseBuffer.length}/${POSE_SAMPLES}`;
      if (poseBuffer.length >= POSE_SAMPLES) {
        galleryVectors[key] = averageVectors(poseBuffer);
        poseBuffer = [];
        // @ts-ignore
        anglesDone[key] = true;
      }
    };

    // Captura secuencial de extremos
    if (!anglesDone.left && yaw < -0.16) captureAngle('left');
    else if (!anglesDone.right && yaw > 0.16) captureAngle('right');
    else if (!anglesDone.up && pitch > 0.13) captureAngle('up');
    else if (!anglesDone.down && pitch < -0.13) captureAngle('down');

    const sidesDone = [anglesDone.left, anglesDone.right, anglesDone.up, anglesDone.down].filter(v => v).length;
    
    // Lógica de estabilización previa al parpadeo
    if (sidesDone === 4) {
      const isCentered = Math.abs(yaw) < 0.05 && Math.abs(pitch) < 0.05;
      if (isCentered) {
        if (!stabilityStartTime) stabilityStartTime = Date.now();
        if (Date.now() - stabilityStartTime >= 1000) {
          isStabilized = true;
          status = "ESTABLE. PARPADEA 2 VECES";
        } else {
          status = "ESTABILIZANDO ROSTRO...";
        }
      } else {
        stabilityStartTime = null;
        isStabilized = false;
        status = "MIRA AL CENTRO";
      }
    }
    regProgress = (sidesDone / 5) * 100;
  }

  async function finalizeRegistration() {
    // El centro también se promedia para mayor calidad
    const centerAvg = averageVectors([getFeatureVector(rawLandmarks)]);
    galleryVectors['center'] = centerAvg;
    anglesDone.center = true;
    regProgress = 100;

    const photo = await captureThumb();
    const newUser = { 
      id: crypto.randomUUID(), 
      name: `USER_${Math.random().toString(36).substr(2, 4).toUpperCase()}`, 
      gallery: { ...galleryVectors }, // Guardamos el set completo de poses
      photo: photo
    };
    
    const db = JSON.parse(localStorage.getItem(DB_KEY) || "[]");
    db.push(newUser);
    localStorage.setItem(DB_KEY, JSON.stringify(db));
    authorizedUser = newUser;
    isRegistering = false;
    status = "REGISTRO COMPLETADO";
  }

  // --- RESTO DE FUNCIONES (Siguen igual para no romper UI) ---
  async function setupCamera() {
    const stream = await navigator.mediaDevices.getUserMedia({ video: { width: 1280, height: 720 } });
    if (videoElement) { 
        videoElement.srcObject = stream; 
        videoElement.onloadedmetadata = () => { 
          videoElement?.play(); 
          requestAnimationFrame(predict); 
        }; 
    }
  }

  async function predict() {
    if (!videoElement || !faceLandmarker) return;
    const results = faceLandmarker.detectForVideo(videoElement, performance.now());

    if (results.faceLandmarks?.length > 0) {
      rawLandmarks = results.faceLandmarks[0];
      
      // --- UI Y TRACKING ---
      points = rawLandmarks.map(p => ({ x: p.x * 100, y: p.y * 100, z: p.z }));
      leftCircle = { x: rawLandmarks[468].x * 100, y: rawLandmarks[468].y * 100, active: true };
      rightCircle = { x: rawLandmarks[473].x * 100, y: rawLandmarks[473].y * 100, active: true };
      
      const xs = points.map(p => p.x);
      const ys = points.map(p => p.y);
      faceBox = { x: Math.min(...xs), y: Math.min(...ys), w: Math.max(...xs) - Math.min(...xs), h: Math.max(...ys) - Math.min(...ys) };
      eyesLocked = true;

      // --- DETECCIÓN DE BLENDSHAPES ---
      const blend = results.faceBlendshapes[0]?.categories || [];
      const blinkL = blend.find(s => s.categoryName === "eyeBlinkLeft")?.score || 0;
      const blinkR = blend.find(s => s.categoryName === "eyeBlinkRight")?.score || 0;
      
      // Lógica de parpadeo sincronizada
      if (blinkL > 0.5 && blinkR > 0.5) {
          // CAPTURA INMEDIATA: Llenamos el buffer mientras el ojo está cerrado
          if (!isProcessing && !authorizedUser && !isRegistering) {
              const currentVector = getFeatureVector(rawLandmarks);
              if (frameBuffer.length < SAMPLES_REQUIRED) {
                  frameBuffer.push(currentVector);
              }
          }

          if (!isBlinking) { 
            isBlinking = true; 
            blinkCount++; 
            
            // Verificamos si ya llegamos a los 2 parpadeos
            if (blinkCount >= 2) {
                if (isRegistering && isStabilized) {
                    finalizeRegistration();
                } else if (!isRegistering && !authorizedUser) {
                    // Si por velocidad el buffer no se llenó, forzamos la última muestra
                    if (frameBuffer.length < SAMPLES_REQUIRED) {
                      frameBuffer.push(getFeatureVector(rawLandmarks));
                    }
                    handleBiometricCheck();
                }
            }
          }
      } else { 
          isBlinking = false; 
      }

      if (isRegistering && !anglesDone.center) handleRegistrationPose(rawLandmarks);

    } else { 
        eyesLocked = false; 
        leftCircle.active = false; 
        rightCircle.active = false; 
        if (!isProcessing) frameBuffer = []; 
        blinkCount = 0;
        stabilityStartTime = null;
    }
    requestAnimationFrame(predict);
  }

  async function captureThumb(): Promise<string> {
    const canvas = document.createElement('canvas');
    canvas.width = 160; canvas.height = 160;
    const ctx = canvas.getContext('2d');
    if (videoElement && ctx) ctx.drawImage(videoElement, videoElement.videoWidth/2 - 360, 0, 720, 720, 0, 0, 160, 160);
    return canvas.toDataURL('image/jpeg', 0.8);
  }

  function startPoseRegistration() {
    isRegistering = true;
    galleryVectors = {};
    poseBuffer = [];
    regProgress = 0;
    blinkCount = 0;
    isStabilized = false;
    stabilityStartTime = null;
    anglesDone = { left: false, right: false, up: false, down: false, center: false };
    status = "INICIANDO CALIBRACIÓN";
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
                        <div class="tag-label">USUARIO AUTORIZADO</div>
                        <div class="tag-name">{authorizedUser.name}</div>
                        <div class="tag-hash">ID: {authorizedUser.id?.substring(0, 8)}</div>
                    </div>
                </div>
            </div>
        {/if}
    </div>

    {#if isRegistering && nextGoal()}
      <div class="registration-guide">
        <div class="arrow-indicator">{nextGoal().icon}</div>
        <div class="progress-mini">
           {#each Object.values(anglesDone) as done}
             <div class="dot" class:done></div>
           {/each}
        </div>
      </div>
    {/if}

    <svg class="biometric-mesh" viewBox="0 0 100 100" preserveAspectRatio="none">
        {#if eyesLocked && !authorizedUser}
            {#each points as p}
                <circle 
                      cx="{p.x}" 
                      cy="{p.y}" 
                      r={0.18 - (p.z * 0.5)} 
                      fill={isProcessing ? "#ff00ff" : "#00ffff"} 
                      opacity={isProcessing ? 0.8 : (0.4 - p.z)} 
                />
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
                {isProcessing ? 'PROCESANDO VECTOR...' : 'TEST DE VIDA (PARPADEA)'}
            </span>
        </div>
      {:else}
        <button class="action-btn" onclick={() => { authorizedUser = null; blinkCount = 0; status = "SISTEMA ONLINE"; }}>
            REINICIAR ESCÁNER
        </button>
      {/if}
    </div>
  </div>
</div>

<style>
  :global(body) { 
    background: #020202; 
    margin: 0; 
    font-family: 'Share Tech Mono', monospace; 
    overflow: hidden; 
  }
  
  .main-container { 
    width: 100vw; 
    height: 100vh; 
    display: flex; 
    align-items: center; 
    justify-content: center; 
  }

  .scanner-frame { 
    position: relative; 
    width: 850px; 
    aspect-ratio: 16 / 9; 
    border: 1px solid rgba(0, 255, 255, 0.2); 
    background: #000; 
    box-shadow: 0 0 40px rgba(0,0,0,0.5);
  }

  video { 
    position: absolute;
    inset: 0;
    width: 100%; 
    height: 100%; 
    object-fit: fill; 
    transform: scaleX(-1) scale(var(--zoom)); 
    transition: transform 1.2s cubic-bezier(0.19, 1, 0.22, 1); 
    filter: brightness(0.7) contrast(1.1); 
  }

  .dynamic-geometry, .biometric-mesh { 
    position: absolute; 
    inset: 0; 
    width: 100%;
    height: 100%;
    pointer-events: none; 
    z-index: 10; 
    /* Mantenemos el espejo para que los puntos coincidan con la cara del video */
    transform: scaleX(-1) scale(var(--zoom)); 
    transition: transform 1.2s cubic-bezier(0.19, 1, 0.22, 1); 
  }

  /* Des-espejamos el tag para que el texto sea legible */
  .floating-tag { 
    position: absolute; 
    transform: translate(-105%, -40%) scaleX(-1); 
    z-index: 50; 
  }

  .tag-body { 
    background: rgba(0, 15, 15, 0.9); 
    border-left: 3px solid #0ff; 
    padding: 10px; 
    display: flex; 
    gap: 12px; 
    backdrop-filter: blur(10px); 
    border-radius: 0 4px 4px 0;
  }

  .tag-body img { width: 60px; height: 60px; border: 1px solid #0ff; object-fit: cover; }
  .tag-details { color: #0ff; }
  .tag-name { font-size: 14px; font-weight: bold; text-transform: uppercase; }
  .tag-label { font-size: 9px; opacity: 0.7; letter-spacing: 1px; }
  .tag-hash { font-size: 8px; margin-top: 4px; opacity: 0.5; }

  /* GUÍA DE REGISTRO: Invertimos el scaleX para corregir el espejo del video */
 .registration-guide {
    position: absolute;
    top: 50%;
    left: 50%;
    /* IMPORTANTE: 
       1. translate(-50%, -50%) centra el div.
       2. scaleX(-1) voltea horizontalmente el contenido para neutralizar el espejo del video.
    */
    transform: translate(-50%, -50%) scaleX(-1); 
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    z-index: 200;
    pointer-events: none;
  }



  .arrow-indicator {
    font-size: 70px;
    color: #0ff;
    text-shadow: 0 0 15px #0ff;
    animation: pulseArrow 0.8s infinite alternate;
  }

  .goal-text {
    color: #0ff;
    font-size: 18px;
    letter-spacing: 2px;
    background: rgba(0, 0, 0, 0.8);
    padding: 10px 20px;
    border: 1px solid #0ff;
    white-space: nowrap; /* Evita que el texto se rompa en dos líneas */
    text-transform: uppercase;
  }
  .progress-mini { display: flex; gap: 8px; margin-top: 15px; }
  .progress-mini .dot { width: 8px; height: 8px; border: 1px solid #0ff; border-radius: 50%; transition: all 0.3s; }
  .progress-mini .dot.done { background: #0ff; box-shadow: 0 0 8px #0ff; transform: scale(1.2); }

  .tracker { 
    position: absolute; 
    width: 18px; height: 18px; 
    border: 1px solid rgba(0, 255, 255, 0.4); 
    transform: translate(-50%, -50%); 
    border-radius: 50%; 
  }
  .tracker.locked { border-color: #0ff; box-shadow: 0 0 10px #0ff; }

  .hud { 
    position: absolute; 
    bottom: 30px; 
    width: 100%; 
    display: flex; 
    flex-direction: column; 
    align-items: center; 
    z-index: 100; 
  }

  .status-box { color: #0ff; letter-spacing: 4px; font-size: 13px; margin-bottom: 15px; text-transform: uppercase; }
  .blink-progress { display: flex; gap: 10px; align-items: center; }
  .progress-segment { width: 35px; height: 5px; border: 1px solid rgba(0,255,255,0.3); transition: all 0.4s; }
  .progress-segment.fill { background: #0ff; box-shadow: 0 0 12px #0ff; border-color: #0ff; }
  .progress-label { color: #0ff; font-size: 10px; margin-left: 12px; opacity: 0.8; }

  .action-btn { 
    background: #0ff; color: #000; border: none; padding: 10px 25px; 
    cursor: pointer; font-family: inherit; font-weight: bold; 
    letter-spacing: 1px; transition: all 0.2s;
  }
  .action-btn:hover { background: #fff; box-shadow: 0 0 15px #fff; }

  @keyframes pulseArrow {
    from { transform: scale(1); opacity: 0.7; }
    to { transform: scale(1.15); opacity: 1; }
  }
</style>