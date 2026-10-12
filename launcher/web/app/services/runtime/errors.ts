export class RuntimeSessionSupersededError extends Error {
  override name = 'AbortError';
  constructor() {
    super('EAGLER_RUNTIME_SESSION_SUPERSEDED');
  }
}
export class RuntimeOperationError extends Error {
  errno?: number;
}
export class LocalOggBytesError extends Error {}

export class RuntimeWaitTimeoutError extends Error {
  override name = 'RuntimeWaitTimeoutError';
  constructor(
    readonly phase: 'ready' | 'first-frame',
    message: string,
  ) {
    super(message);
  }
}
