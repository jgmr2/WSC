<script>
  import Editor from "../../lib/editor.svelte";
</script>

<svelte:head>
  <title>Editor LaTeX | Mi App</title>
</svelte:head>

<Editor />

<style>
  /* Puedes añadir estilos globales para la página aquí si quieres */
  :global(body) {
    margin: 0;
    padding: 0;
    overflow: hidden; /* Evita scrolls dobles si el editor mide 100vh */
  }
</style>