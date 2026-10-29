import { join } from 'path';

function display(value) {
  return join('/tmp', value);
}

class Widget {
  render(value) {
    return display(value);
  }
}

console.log(display('example'));
