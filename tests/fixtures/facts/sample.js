import { join } from 'path';

class Widget {
  render(value) {
    const target = join('/tmp', value);
    return target.trim();
  }
}

function create(value) {
  let local = value;
  return new Widget().render(local);
}
