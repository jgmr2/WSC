<script>
    let message = "Esto es un mensaje desde svelte";
</script>

<div>
    <h1>{message}</h1>
</div>

<style>
    /* Corrección de :global */
    :global(body) {
        background-color: black;
        margin: 0;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
    }

    h1 {
        /* Cambiado a blanco para que sea legible sobre negro */
        color: rgb(200, 255, 255); 
        font-family: sans-serif;
    }
</style>