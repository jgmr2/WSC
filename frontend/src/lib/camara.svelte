<script>
  const wasmPath = '/camara.js';
  
  // Cargamos el módulo directamente en una promesa
  const motor = import(/* @vite-ignore */ wasmPath)
    .then(m => m.default({ locateFile: p => `/${p}` }));
</script>

{#await motor}
  <h1>Esperando...</h1>
{:then wasm}
  <h1>Motor listo ✅</h1>
{:catch}
  <h1>Error al cargar WASM</h1>
{/await}