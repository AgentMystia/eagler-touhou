import { startTransition, StrictMode } from 'react';
import { hydrateRoot } from 'react-dom/client';
import { HydratedRouter } from 'react-router/dom';
if (new URLSearchParams(location.search).has('pixelExperiment')) {
  const script = document.createElement('script');
  script.type = 'module';
  script.src = new URL('../public/pixel-scaling-experiment.mjs', import.meta.url).href;
  document.head.append(script);
}
startTransition(() => {
  hydrateRoot(
    document,
    <StrictMode>
      <HydratedRouter />
    </StrictMode>,
  );
});
