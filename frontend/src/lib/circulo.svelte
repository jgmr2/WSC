<script>
  import { onMount } from 'svelte';

  // Variables de estado
  let wasmInstance;
  let r = 255, g = 0, b = 0;
  let loaded = false;

  onMount(async () => {
    try {
      // Cargamos el pegamento de Emscripten desde /static/logic.js
      // Usamos import dinámico para que Vite no intente procesarlo
      const modulePath = '/main.js';
      const { default: initWasm } = await import(/* @vite-ignore */ modulePath);
      
      // Inicializamos
      wasmInstance = await initWasm();
      loaded = true;
      
      // Leemos el color inicial desde C++
      syncColor();
    } catch (e) {
      console.error("Error cargando WASM en el componente:", e);
    }
  });

  function syncColor() {
    r = wasmInstance._get_r();
    g = wasmInstance._get_g();
    b = wasmInstance._get_b();
  }

  function handleClick() {
    if (!wasmInstance) return;
    wasmInstance._handle_click(); // Lógica en C++
    syncColor(); // Traer nuevos datos a Svelte
  }
</script>

<div class="circle-container">
  {#if !loaded}
    <div class="skeleton"></div>
  {:else}
    <button 
      on:click={handleClick} 
      aria-label="Cambiar color"
      style="--circle-color: rgb({r}, {g}, {b})"
    >
      <svg width="150" height="150">
        <circle cx="75" cy="75" r="70" />
      </svg>
    </button>
  {/if}
</div>

<style>
  .circle-container {
    display: inline-block;
    margin: 1rem;
  }

  button {
    background: none;
    border: none;
    cursor: pointer;
    padding: 0;
    transition: transform 0.1s ease;
  }

  button:active {
    transform: scale(0.95);
  }

  circle {
    fill: var(--circle-color);
    stroke: #333;
    stroke-width: 4;
    transition: fill 0.3s ease;
  }

  .skeleton {
    width: 150px;
    height: 150px;
    background: #eee;
    border-radius: 50%;
    animation: pulse .5s infinite;
  }

  @keyframes pulse {
    0% { opacity: 0.5; }
    50% { opacity: 1; }
    100% { opacity: 0.5; }
  }
</style>