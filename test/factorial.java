public class factorial
{
    public static void main(String[] args)
    {
        if(args.length == 0)
        {
            System.out.println("Wrong args");
            return;
        }

        System.out.println("Factorial " + args[0] + " = " +
                            jit(Integer.parseInt(args[0])));
    }

    public static int jit(int i)
    {
        if(i == 0)
        {
            return 1;
        }

        return i * intrp(i - 1);
    }

    public static int intrp(int i)
    {
        if(i == 0)
        {
            return 1;
        }

        return i * jit(i - 1);
    }
}
