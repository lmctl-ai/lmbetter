import java.security.MessageDigest;
class Violations {
  String password = "hard-coded-secret";
  void run(String command) throws Exception {
    Exception e = new Exception();
    try { work(); } catch (Exception e) { }
    System.out.println("debug");
    if ("left" == "right") { e.printStackTrace(); }
    Runtime.getRuntime().exec(command);
    MessageDigest.getInstance("MD5");
    try { work(); } catch (Throwable t) { throw new RuntimeException(t); }
    // TODO: fix this FIXME later
  }
  void work() throws java.io.IOException { }
}
