export function parseOptions(argv, { values = [], flags = [] } = {}) {
  const result = {};
  for (const raw of argv) {
    const match = /^--([a-z][a-z0-9-]*)(?:=(.*))?$/.exec(raw);
    if (!match || Object.hasOwn(result, match[1])) throw new Error('Invalid or duplicate argument: ' + raw);
    const [, name, value] = match;
    if (flags.includes(name)) {
      if (value !== undefined && !['1', '0', 'true', 'false'].includes(value))
        throw new Error('Expected a boolean flag: ' + raw);
      result[name] = value === undefined || value === '1' || value === 'true';
    } else if (values.includes(name) && value !== undefined && value.trim()) {
      result[name] = value;
    } else throw new Error('Unknown option or missing value: ' + raw);
  }
  return result;
}
