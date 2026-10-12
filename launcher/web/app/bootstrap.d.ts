declare module 'virtual:launcher-bootstrap' {
  interface BootstrapSource {
    script: string;
    compatibility: string;
    css: string;
    preload: string;
  }
  export const bootstrapAssets: { library: BootstrapSource; lobby: BootstrapSource };
}
