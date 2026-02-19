<script>
  import { onMount, onDestroy } from 'svelte';

  let latexCode = `\\documentclass{article}\n\\begin{document}\n\\section{Hola desde C++}\nEste PDF fue generado usando WASM y Embind.\n\\end{document}`;
  
  let pdfUrl = null;
  let isCompiling = false;
  let wasmModule = null;
  let errorMessage = "";
  
  // Tema inicial basado en preferencia del sistema
  let theme = 'dark'; 
  
  // Lógica de Redimensionamiento
  let editorWidth = 50;
  let isResizing = false;

  const handleMouseMove = (e) => {
    if (!isResizing) return;
    const newWidth = (e.clientX / window.innerWidth) * 100;
    if (newWidth > 15 && newWidth < 85) editorWidth = newWidth;
  };

  onMount(async () => {
    // 1. Detección automática de tema del sistema
    const themeMediaQuery = window.matchMedia('(prefers-color-scheme: light)');
    
    // Función para actualizar el estado del tema
    const updateTheme = (e) => theme = e.matches ? 'light' : 'dark';
    
    // Escuchar cambios en tiempo real
    themeMediaQuery.addEventListener('change', updateTheme);
    
    // Establecer tema inicial
    updateTheme(themeMediaQuery);

    // Eventos de redimensionamiento
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', () => isResizing = false);

    try {
      const wasmJsPath = '/latexengine.js';
      const moduleLoader = await import(/* @vite-ignore */ wasmJsPath);
      wasmModule = await moduleLoader.default({ locateFile: (path) => `/${path}` });
    } catch (e) {
      errorMessage = "bash: wasm-engine: link failed";
    }
  });

  onDestroy(() => {
    // Limpieza de listeners
    if (typeof window !== 'undefined') {
      window.removeEventListener('mousemove', handleMouseMove);
    }
    if (pdfUrl) URL.revokeObjectURL(pdfUrl);
  });

  async function compile() {
    if (!wasmModule || isCompiling) return;
    isCompiling = true;
    errorMessage = "";
    try {
      const pdfVector = wasmModule.compileLatex(latexCode);
      const size = pdfVector.size();
      const buffer = new Uint8Array(size);
      for (let i = 0; i < size; i++) buffer[i] = pdfVector.get(i);
      pdfVector.delete();
      if (pdfUrl) URL.revokeObjectURL(pdfUrl);
      pdfUrl = URL.createObjectURL(new Blob([buffer.buffer], { type: 'application/pdf' }));
    } catch (err) {
      errorMessage = `fatal: compilation error`;
    } finally {
      isCompiling = false;
    }
  }
</script>

<main class="app-container" data-theme={theme} class:is-resizing={isResizing}>
  <div class="terminal-layout" style="grid-template-columns: {editorWidth}% 8px 1fr;">
    
    <section class="panel editor-panel">
      <header class="terminal-header">
        <div class="window-controls">
          <span class="dot red"></span>
          <span class="dot yellow"></span>
          <span class="dot green"></span>
        </div>
        <div class="tab">
          <span class="icon">$_</span> <span class="filename">main.tex</span>
        </div>
        <div class="system-status">
            {theme === 'dark' ? 'BLUE_HOUR_ACTIVE' : 'GOLDEN_HOUR_ACTIVE'}
        </div>
      </header>

      <div class="editor-content">
        <div class="line-numbers">
          {#each Array(25) as _, i}<span>{i + 1}</span>{/each}
        </div>
        <textarea bind:value={latexCode} spellcheck="false"></textarea>
      </div>

      <footer class="action-bar">
        <div class="status">
          <span class="prompt">user@wasm:~/latex$</span>
          <span class={isCompiling ? 'blink' : 'ready'}>
            {isCompiling ? 'compiling...' : 'ready'}
          </span>
        </div>
        <button class="btn-run" on:click={compile} disabled={isCompiling || !wasmModule}>
          ./render.sh
        </button>
      </footer>
    </section>

    <div class="resizer" on:mousedown={() => isResizing = true}></div>

    <section class="panel preview-panel">
      <header class="terminal-header secondary">
        <div class="tab active">
          <span class="icon">>_</span> <span class="filename">output.pdf</span>
        </div>
      </header>

      <div class="viewer-container">
        {#if errorMessage}
          <div class="terminal-error"><p>[ERROR] {errorMessage}</p></div>
        {/if}

        {#if pdfUrl}
          {#key pdfUrl}
            <iframe src={pdfUrl} title="PDF Preview"></iframe>
          {/key}
        {:else if !errorMessage}
          <div class="terminal-placeholder">
            <p class="blink-cursor">> STDOUT: Waiting for build instructions...</p>
          </div>
        {/if}
      </div>
    </section>
  </div>
</main>

<style>
  /* Paletas de colores Blue Hour & Golden Hour */
  :root {
    --bh-bg: #0a0b1e; --bh-panel: #14162d; --bh-header: #1c1e3f;
    --bh-text: #a5b4fc; --bh-accent: #22d3ee; --bh-border: #2e3267;
    --bh-prompt: #818cf8;
    
    --gh-bg: #fffbf5; --gh-panel: #ffedd5; --gh-header: #fed7aa;
    --gh-text: #7c2d12; --gh-accent: #ea580c; --gh-border: #fdba74;
    --gh-prompt: #9a3412;

    --font-mono: 'JetBrains Mono', 'Fira Code', 'Menlo', monospace;
  }

  [data-theme='dark'] {
    --bg: var(--bh-bg); --panel: var(--bh-panel); --header: var(--bh-header);
    --text: var(--bh-text); --accent: var(--bh-accent); --border: var(--bh-border);
    --prompt: var(--bh-prompt); --subtle: #312e81;
  }

  [data-theme='light'] {
    --bg: var(--gh-bg); --panel: var(--gh-panel); --header: var(--gh-header);
    --text: var(--gh-text); --accent: var(--gh-accent); --border: var(--gh-border);
    --prompt: var(--gh-prompt); --subtle: #fbbf24;
  }

  :global(body) {
    margin: 0; padding: 0;
    background-color: var(--bg);
    color: var(--text);
    font-family: var(--font-mono);
    overflow: hidden;
    transition: background 0.4s ease, color 0.4s ease;
  }

  .app-container {
    height: 100vh;
    display: flex;
    padding: 0.75rem;
    box-sizing: border-box;
  }

  .terminal-layout {
    display: grid;
    width: 100%;
    gap: 0;
  }

  .resizer {
    width: 8px;
    cursor: col-resize;
    z-index: 10;
  }
  .resizer:hover { background: var(--accent); opacity: 0.2; }

  .is-resizing { user-select: none; cursor: col-resize; }
  .is-resizing iframe { pointer-events: none; }

  .panel {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 6px;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-shadow: 0 4px 20px rgba(0,0,0,0.3);
  }

  .terminal-header {
    background: var(--header);
    padding: 0.6rem 1rem;
    display: flex;
    align-items: center;
    gap: 1.2rem;
  }

  .system-status {
    margin-left: auto;
    font-size: 0.6rem;
    letter-spacing: 1px;
    opacity: 0.6;
    border: 1px solid var(--accent);
    padding: 2px 6px;
    border-radius: 3px;
    color: var(--accent);
  }

  .tab {
    background: var(--panel);
    padding: 0.4rem 1rem;
    font-size: 0.75rem;
    color: var(--accent);
    border: 1px solid var(--border);
    border-bottom: none;
    border-radius: 4px 4px 0 0;
  }

  .editor-content { flex: 1; display: flex; }

  .line-numbers {
    padding: 1.2rem 0.5rem;
    background: rgba(0,0,0,0.2);
    color: var(--subtle);
    text-align: right;
    font-size: 0.7rem;
    min-width: 30px;
    user-select: none;
  }

  textarea {
    flex: 1;
    background: transparent;
    color: var(--text);
    border: none;
    padding: 1.2rem;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    resize: none;
    outline: none;
    line-height: 1.6;
  }

  .action-bar {
    background: var(--header);
    padding: 0.6rem 1rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .status { font-size: 0.8rem; }
  .prompt { color: var(--prompt); margin-right: 5px; }

  .btn-run {
    background: var(--accent);
    color: var(--bg);
    border: none;
    padding: 0.5rem 1.2rem;
    font-family: var(--font-mono);
    font-weight: bold;
    cursor: pointer;
    border-radius: 4px;
    transition: transform 0.1s;
  }
  .btn-run:active { transform: scale(0.96); }

  .viewer-container {
    flex: 1;
    background: #333;
  }

  iframe { width: 100%; height: 100%; border: none; }

  .terminal-placeholder {
    padding: 2rem;
    color: var(--accent);
    background: var(--bg);
    height: 100%;
  }

  .blink { animation: blinker 0.8s linear infinite; color: var(--accent); }
  @keyframes blinker { 50% { opacity: 0; } }
</style>