interface Props { name: string; }

export function Greeting(props: Props) {
  return <div className="hello">Hello {props.name}</div>;
}
