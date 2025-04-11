class ReverseMe {
   public static void main(String[] PassedArguments) {
      if (PassedArguments.length != 3) {
         System.out.println("You need to pass this program 3 args!");
      } else {
         if (PassedArguments[0].equals("Java") && PassedArguments[1].equals("is easily") && arg3check(PassedArguments[2])) {
            String Drinks = String.join(" ", PassedArguments);
            printFlag(Drinks);
         } else {
            System.out.println("Better luck next time");
         }

      }
   }




   public static String right_to_left(String PassedArguments) {
      String Drinks = "";

      for(int randomString = PassedArguments.length() - 1; randomString >= 0; --randomString) {
         Drinks = Drinks + PassedArguments.charAt(randomString);
      }

      return Drinks;
   }

   public static boolean arg3check(String PassedArguments) {
      String Drinks = "delephantsrelephantvelephantr";
      String randomString = Drinks.replaceAll("elephant", "e");
      if (PassedArguments.equals(right_to_left(randomString))) {
         return true;
      } else {
         System.out.println(right_to_left("This is confusing = " + PassedArguments));
         return false;
      }
   }

   public static void printFlag(String PassedArguments) {
      String Drinks = "";
      int[] randomString = new int[]{61, 8, 26, 5, 67, 8, 7, 91, 9, 80, 5, 0, 2, 38, 84, 26, 86, 41, 2, 0, 66, 1, 6, 126, 6, 41, 13, 17, 15, 22, 93};
      int var3 = 0;

      for(int var4 = 0; var4 < randomString.length; ++var4) {
         char var5 = PassedArguments.charAt(var3);
         int var6 = randomString[var4] ^ var5;
         Drinks = Drinks + (char)var6;
         ++var3;
         if (var3 == PassedArguments.length()) {
            var3 = 0;
         }
      }

      System.out.println("Flag: " + Drinks);
   }
}
