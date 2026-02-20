<script>
  import { onMount } from 'svelte';
  let wasmModule;
  let mensajeDesdeWasm = "Esperando...";

  onMount(async () => {
    try {
      const wasmJsPath = '/mensaje.js'; 
      const moduleLoader = await import(/* @vite-ignore */ wasmJsPath);
      
      wasmModule = await moduleLoader.default({ 
        locateFile: (path) => `/${path}` 
      });

      // Llamada inicial
      //obtenerTexto();
    } catch (e) {
      mensajeDesdeWasm = "Error al cargar WASM";
    }
  });

  function obtenerTexto() {
    if (wasmModule) {
      // Cada vez que llamamos, C++ cambia su variable interna y nos da el nuevo texto
      mensajeDesdeWasm = wasmModule.ccall('mensaje', 'string', [], []);
    }
  }
</script>

<div class="terminal">
  <h1>Control de Estado en C++</h1>
  
  <div class="monitor">
    <p>{mensajeDesdeWasm}</p>
  </div>

  <button on:click={obtenerTexto}>
    Alternar Estado (Toggle en C++)
  </button>
</div>

<style>
  .terminal { text-align: center; margin-top: 50px; font-family: monospace; }
  .monitor { 
    background: #222; color: #0f0; padding: 20px; 
    border-radius: 5px; display: inline-block; margin-bottom: 20px;
  }
  button { cursor: pointer; padding: 10px 20px; }
</style>