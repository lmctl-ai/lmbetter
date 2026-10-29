import { join } from 'path';

interface Named { name: string; }
type Label = string;

class Widget {
  render(value: string): string {
    return join('/tmp', value);
  }
}

function display(value: string): string {
  const label: Label = value;
  return label.trim();
}

const example: Named = { name: display('typescript') };
console.log(new Widget().render(example.name));
