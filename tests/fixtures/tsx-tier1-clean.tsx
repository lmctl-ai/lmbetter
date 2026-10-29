import { join } from 'path';

function display(value: string): string {
  return join('/tmp', value);
}

const view = <div>{display('safe')}</div>;
