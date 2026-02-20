<script>
  import { onMount } from 'svelte';

  let wasmModule;
  let status = "Cargando motor...";
  let ejecutando = false;

  // Estados para WASM
  let tiempoWasm = 0;
  let resultadoPiWasm = 0;

  // Estados para JavaScript
  let tiempoJS = 0;
  let resultadoPiJS = 0;

  const ITERACIONES = 10_000_000;

  onMount(async () => {
    try {
      const wasmJsPath = '/montecarlo.js'; 
      const moduleLoader = await import(/* @vite-ignore */ wasmJsPath);
      
      wasmModule = await moduleLoader.default({ 
        locateFile: (path) => `/${path}` 
      });

      status = "Motores listos";
    } catch (e) {
      status = "Error al cargar WASM: " + e.message;
    }
  });

  // --- Lógica en JavaScript Puro ---
  function montecarloJS(n) {
    let dentro = 0;
    for (let i = 0; i < n; i++) {
      const x = Math.random();
      const y = Math.random();
      if (x * x + y * y <= 1.0) dentro++;
    }
    return 4.0 * dentro / n;
  }

  async function ejecutarBenchmark() {
    if (!wasmModule) return;
    ejecutando = true;

    // Pequeña pausa para dejar que la UI respire antes del cálculo pesado
    await new Promise(r => setTimeout(r, 100));

    // 1. Test de JavaScript
    const startJS = performance.now();
    resultadoPiJS = montecarloJS(ITERACIONES);
    tiempoJS = performance.now() - startJS;

    // 2. Test de WebAssembly
    const startWasm = performance.now();
    resultadoPiWasm = wasmModule.ccall(
      'test_montecarlo', 
      'number', 
      ['number'], 
      [ITERACIONES]
    );
    tiempoWasm = performance.now() - startWasm;

    ejecutando = false;
  }
</script>

<div class="benchmark">
  <h1>Benchmark: JS vs WebAssembly</h1>
  <p class="status">Estado: <strong>{status}</strong></p>
  <p>Calculando π con {ITERACIONES.toLocaleString()} puntos</p>

  <div class="grid">
    <div class="card js">
      <h2>JavaScript</h2>
      <div class="result">π ≈ {resultadoPiJS.toFixed(6)}</div>
      <div class="time">{tiempoJS.toFixed(2)} <span>ms</span></div>
    </div>

    <div class="card wasm">
      <h2>WASM (C++)</h2>
      <div class="result">π ≈ {resultadoPiWasm.toFixed(6)}</div>
      <div class="time">{tiempoWasm.toFixed(2)} <span>ms</span></div>
    </div>
  </div>

  {#if tiempoWasm > 0 && tiempoJS > 0}
    <div class="comparison">
      Wasm es <strong>{(tiempoJS / tiempoWasm).toFixed(1)}x</strong> más rápido que JS
    </div>
  {/if}

  <button on:click={ejecutarBenchmark} disabled={ejecutando || !wasmModule}>
    {ejecutando ? 'Procesando...' : 'Iniciar Duelo'}
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
  .result { font-size: 1.1rem; margin-bottom: 0.5rem; }
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