class ReverseMe {
   public static void main(String[] var0) {
      if (var0.length != 3) {
         System.out.println("You need to pass this program 3 args!");
      } else {
         if (arg1check(var0[0]) && arg2check(var0[1]) && arg3check(var0[2])) {
            String var1 = String.join(" ", var0);
            printFlag(var1);
         } else {
            System.out.println("Better luck next time");
         }

      }
   }

   public static boolean arg1check(String var0) {
      String[] var1 = new String[]{"Ice_Coffee", "Frappe", "Java", "Cocoa", "Rust", "Sea"};
      if (!var0.equals(var1[2])) {
         System.out.println("My favorite programming language to drink is not " + var0);
         return false;
      } else {
         return true;
      }
   }

   public static boolean arg2check(String var0) {
      String var1 = Character.toString((char)Integer.decode("0163"));
      String var2 = "extra_feasilyiest_wonderfulmustwhy_stinkers";
      String var3 = var2.substring(7, 13);
      String var4 = "i" + var1 + " " + var3;
      if (var0.equals(var4)) {
         return true;
      } else {
         System.out.println("My feelings are not currently = " + var0);
         return false;
      }
   }

   public static String right_to_left(String var0) {
      String var1 = "";

      for(int var2 = var0.length() - 1; var2 >= 0; --var2) {
         var1 = var1 + var0.charAt(var2);
      }

      return var1;
   }

   public static boolean arg3check(String var0) {
      String var1 = "delephantsrelephantvelephantr";
      String var2 = var1.replaceAll("elephant", "e");
      if (var0.equals(right_to_left(var2))) {
         return true;
      } else {
         System.out.println(right_to_left("This is confusing = " + var0));
         return false;
      }
   }

   public static void printFlag(String var0) {
      String var1 = "";
      int[] var2 = new int[]{61, 8, 26, 5, 67, 8, 7, 91, 9, 80, 5, 0, 2, 38, 84, 26, 86, 41, 2, 0, 66, 1, 6, 126, 6, 41, 13, 17, 15, 22, 93};
      int var3 = 0;

      for(int var4 = 0; var4 < var2.length; ++var4) {
         char var5 = var0.charAt(var3);
         int var6 = var2[var4] ^ var5;
         var1 = var1 + (char)var6;
         ++var3;
         if (var3 == var0.length()) {
            var3 = 0;
         }
      }

      System.out.println("Flag: " + var1);
   }
}
