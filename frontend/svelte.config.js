import adapter from '@sveltejs/adapter-static';
import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';

/** @type {import('@sveltejs/kit').Config} */
const config = {
	preprocess: vitePreprocess(),
	kit: {
		// adapter-auto solo funciona en entornos específicos, 
		// adapter-static genera la carpeta 'build'
		adapter: adapter({
			pages: 'build',
			assets: 'build',
			fallback: 'index.html', // Recomendado para Single Page Apps (SPA)
			precompress: false,
			strict: true
		})
	}
};

export default config;