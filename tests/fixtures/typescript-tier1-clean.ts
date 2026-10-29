import { join } from 'path';

function display(value: string): string {
  return join('/tmp', value);
}

console.log(display('safe'));
