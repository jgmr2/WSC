<script>
  import Circulo from '$lib/circulo.svelte';
</script>

<main>
  <h1>Panel de Control Crow++</h1>
  <p>El círculo de abajo es gestionado por un binario de C++ vía WebAssembly.</p>

  <div class="canvas-area">
    <Circulo />
  </div>

  <section class="info">
    <h3>Estado del Sistema:</h3>
    <ul>
      <li>Backend: Crow (C++)</li>
      <li>Frontend: SvelteKit</li>
      <li>Lógica de UI: WASM</li>
    </ul>
  </section>
</main>

<style>
  main {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    max-width: 800px;
    margin: 0 auto;
    padding: 2rem;
    text-align: center;
  }

  .canvas-area {
    background: #fff;
    border: 2px dashed #ccc;
    border-radius: 20px;
    padding: 3rem;
    margin: 2rem 0;
  }

  .info {
    text-align: left;
    display: inline-block;
    background: #f9f9f9;
    padding: 1rem 2rem;
    border-radius: 10px;
  }
</style>