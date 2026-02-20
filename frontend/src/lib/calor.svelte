<script>
  import { onMount } from 'svelte';

  let wasmModule;
  let status = "Cargando motor de calor...";
  let ejecutando = false;

  let tiempoWasm = 0;
  let tiempoJS = 0;

  const SIZE = 500;
  const TOTAL_SIZE = SIZE * SIZE;
  const PASADAS = 100;

  onMount(async () => {
    try {
      const wasmJsPath = '/procesar_calor.js'; 
      const moduleLoader = await import(/* @vite-ignore */ wasmJsPath);
      
      // Inicializamos el módulo
      const instance = await moduleLoader.default({ 
        locateFile: (path) => `/${path}` 
      });

      // Guardamos la instancia
      wasmModule = instance;
      status = "Motores listos";
    } catch (e) {
      console.error(e);
      status = "Error al cargar WASM: " + e.message;
    }
  });

  // --- Lógica JS (Sin cambios) ---
  function stepJS(actual, siguiente) {
    for (let y = 1; y < SIZE - 1; y++) {
      for (let x = 1; x < SIZE - 1; x++) {
        let i = y * SIZE + x;
        siguiente[i] = (actual[i-SIZE-1] + actual[i-SIZE] + actual[i-SIZE+1] +
                        actual[i-1] + actual[i] + actual[i+1] +
                        actual[i+SIZE-1] + actual[i+SIZE] + actual[i+SIZE+1]) / 9;
      }
    }
  }

  async function ejecutarBenchmark() {
    if (!wasmModule || !wasmModule._malloc) {
      status = "Error: Módulo no exporta malloc";
      return;
    }
    
    ejecutando = true;
    const dataA = new Float32Array(TOTAL_SIZE).fill(100);
    const dataB = new Float32Array(TOTAL_SIZE);

    await new Promise(r => setTimeout(r, 100));

    // 1. Test JS
    const startJS = performance.now();
    for(let i = 0; i < PASADAS; i++) {
        stepJS(dataA, dataB);
    }
    tiempoJS = performance.now() - startJS;

    // 2. Test WASM
    try {
        const startWasm = performance.now();
        
        // Reserva de bytes: TOTAL_SIZE * 4 (porque cada Float32 ocupa 4 bytes)
        const bytesPerElement = Float32Array.BYTES_PER_ELEMENT;
        const ptrA = wasmModule._malloc(TOTAL_SIZE * bytesPerElement);
        const ptrB = wasmModule._malloc(TOTAL_SIZE * bytesPerElement);
        
        // IMPORTANTE: Acceder al HEAP a través de la instancia
        // Usamos Math.floor o bitwise para asegurar que el índice es entero
        const heapIndex = ptrA >> 2; // Equivalente a ptrA / 4
        wasmModule.HEAPF32.set(dataA, heapIndex);

        for(let i = 0; i < PASADAS; i++) {
            // Pasamos los punteros (direcciones de memoria)
            wasmModule._procesar_calor(ptrA, ptrB, SIZE, SIZE);
        }

        // Liberar
        wasmModule._free(ptrA);
        wasmModule._free(ptrB);

        tiempoWasm = performance.now() - startWasm;
    } catch (err) {
        console.error("Error en ejecución WASM:", err);
        status = "Error en ejecución";
    }

    ejecutando = false;
  }
</script>

<div class="benchmark">
  <h1>Benchmark: Difusión de Calor</h1>
  <p class="status">Estado: <strong>{status}</strong></p>
  <p>Procesando {SIZE}x{SIZE} celdas ({PASADAS} iteraciones)</p>

  <div class="grid">
    <div class="card js">
      <h2>JavaScript</h2>
      <div class="result">Bucle for anidado</div>
      <div class="time">{tiempoJS.toFixed(2)} <span>ms</span></div>
    </div>

    <div class="card wasm">
      <h2>WASM (C++)</h2>
      <div class="result">Punteros Directos</div>
      <div class="time">{tiempoWasm.toFixed(2)} <span>ms</span></div>
    </div>
  </div>

  {#if tiempoWasm > 0 && tiempoJS > 0}
    <div class="comparison">
      Wasm es <strong>{(tiempoJS / tiempoWasm).toFixed(1)}x</strong> más rápido que JS
    </div>
  {/if}

  <button on:click={ejecutarBenchmark} disabled={ejecutando || !wasmModule}>
    {ejecutando ? 'Calculando física...' : 'Iniciar Stress Test'}
  </button>
</div>

<style>
  .benchmark { text-align: center; font-family: 'Segoe UI', sans-serif; padding: 2rem; max-width: 800px; margin: auto; }
  .status { font-size: 0.9rem; color: #666; }
  .grid { display: flex; gap: 20px; justify-content: center; margin: 2rem 0; }
  
  .card { 
    flex: 1; 
    padding: 1.5rem; 
    border-radius: 12px; 
    background: #fff;
    box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    border-top: 5px solid #ccc;
  }

  .js { border-top-color: #f7df1e; }
  .wasm { border-top-color: #654ff0; }

  h2 { margin-top: 0; font-size: 1.2rem; }
  .result { font-size: 1rem; color: #666; margin-bottom: 0.5rem; }
  .time { font-size: 2rem; font-weight: bold; color: #333; }
  .time span { font-size: 1rem; color: #888; }

  .comparison { 
    background: #e6fffa; 
    color: #2c7a7b; 
    padding: 1rem; 
    border-radius: 8px; 
    font-size: 1.2rem;
    margin-bottom: 2rem;
  }

  button { 
    padding: 1rem 2.5rem; 
    font-size: 1.1rem; 
    font-weight: bold;
    cursor: pointer; 
    background: #222; 
    color: #fff; 
    border: none; 
    border-radius: 50px;
    transition: transform 0.1s;
  }
  
  button:active { transform: scale(0.98); }
  button:disabled { background: #ccc; cursor: not-allowed; }
</style>