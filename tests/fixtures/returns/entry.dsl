$return.body = "kept";
@pair fn first(n: Node) -> void implements Rule {
  let values = str_list();
  values.add("owned");
  $return.captured = values;
  values.add("mutated");
  $return[n.text()] = "visited";
  ts.report(n, "first", "R_FIRST", "warning");
}
@pair fn second(n: Node) -> void implements Rule {
  $return.last = n.text();
  ts.report(n, "second", "R_SECOND", "error");
}
