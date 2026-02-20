<script>
    let message = "Esto es un mensaje desde svelte";
    import Mensaje from "../../lib/mensaje.svelte";
    import Montecarlo from "../../lib/montecarlo.svelte";
    import Calor from "../../lib/calor.svelte";
</script>

<div>
    <Mensaje/>
    <h1>{message}</h1>
    <Montecarlo/>
    <Calor/>
</div>

<style>
    /* Corrección de :global */
    :global(body) {
        background-color: rgb(12, 59, 106);
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