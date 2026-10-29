import { join } from 'path';

function display(value) {
  return join('/tmp', value);
}

console.log(display('safe'));
