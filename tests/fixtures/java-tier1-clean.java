import java.security.MessageDigest;
class Clean {
  private static final String LABEL = "value";
  public static void main(String[] args) { System.out.println("entry point"); }
  void run(String command) throws Exception {
    try { work(); } catch (java.io.IOException e) { recover(e); }
    if (LABEL.equals("value")) { }
    Runtime.getRuntime().exec("fixed-command");
    MessageDigest.getInstance("SHA-256");
    try { work(); } catch (java.io.IOException e) { recover(e); }
    // ordinary documentation comment
  }
  void work() { }
  void recover(Exception e) { }
}
