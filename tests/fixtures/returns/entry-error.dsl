$return.body = "discard";
@pair fn partial(n: Node) -> void implements Rule {
  $return.partial = "discard";
  n.child(999);
}
