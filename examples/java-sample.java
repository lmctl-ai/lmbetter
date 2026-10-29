class JavaSample {
  private String token = "replace-me";
  void execute(String command) {
    try { run(); } catch (Exception e) { }
    System.out.println("debug");
    Runtime.getRuntime().exec(command);
    // TODO: remove this example finding
  }
  void run() { }
}
