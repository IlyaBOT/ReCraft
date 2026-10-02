/* Development-only reader for the unmodified Beta 1.7.3 client registry.
 * javac -d build/recipe-reader tools/ExtractBetaRecipes.java
 * java -cp build/recipe-reader;<vanilla-client.jar> ExtractBetaRecipes
 * No Minecraft code or JAR is copied into the runtime. Output is recipe data. */
import java.lang.reflect.Field;
import java.io.PrintStream;
import java.io.ByteArrayOutputStream;
import java.util.List;

public final class ExtractBetaRecipes {
    static Object field(Object value, String name) throws Exception {
        for (Class<?> type=value.getClass();type!=null;type=type.getSuperclass()) {
            try {
                Field f=type.getDeclaredField(name);
                f.setAccessible(true);
                return f.get(value);
            } catch (NoSuchFieldException ignored) { }
        }
        throw new NoSuchFieldException(name);
    }
    static int integer(Object value, String name) throws Exception {
        return ((Integer) field(value, name)).intValue();
    }
    static String ingredient(Object stack) throws Exception {
        return stack == null ? "{0,0}" : "{" + integer(stack,"c") + "," + integer(stack,"d") + "}";
    }
    public static void main(String[] args) throws Exception {
        PrintStream output = System.out;
        System.setOut(new PrintStream(new ByteArrayOutputStream()));
        // The client initializes blocks before crafting; reversing this order
        // recurses through the original statistics registry during class init.
        Class.forName("uu");
        if (args.length>0 && args[0].equals("--sounds")) {
            Class<?> type=Class.forName("uu"),soundType=Class.forName("ct");
            Object[] blocks=null;
            for (Field f:type.getDeclaredFields()) if (f.getType().isArray() && f.getType().getComponentType()==type) {
                f.setAccessible(true); blocks=(Object[])f.get(null); break;
            }
            System.setOut(output);
            output.println("/* Break/step sound names, volume and pitch read from vanilla Beta uu/ct. */");
            for (int b=0;b<97;++b) if (blocks[b]!=null) {
                Object sound=field(blocks[b],"by");
                output.println("["+b+"]={\""+soundType.getMethod("a").invoke(sound)+"\",\""+
                    soundType.getMethod("d").invoke(sound)+"\","+field(sound,"b")+"f,"+field(sound,"c")+"f},");
            }
            return;
        }
        if (args.length>0 && (args[0].equals("--mining") || args[0].equals("--materials"))) {
            Class<?> blockType=Class.forName("uu"),itemType=Class.forName("gm"),stackType=Class.forName("iz"),materialType=Class.forName("ln");
            Object[] blocks=null;
            for (Field f:blockType.getDeclaredFields()) if (f.getType().isArray() && f.getType().getComponentType()==blockType) {
                f.setAccessible(true); blocks=(Object[])f.get(null); break;
            }
            Object[] items=(Object[])itemType.getField("c").get(null);
            System.setOut(output);
            if (args[0].equals("--materials")) {
                Object wood=materialType.getField("d").get(null);
                output.println("/* Material flags: solid, blocks flow, harvestable, burns, wood. Vanilla Beta registry. */");
                for (int b=1;b<97;++b) if (blocks[b]!=null) {
                    Object mat=field(blocks[b],"bA");
                    output.print("["+b+"]={");
                    String[] methods={"a","c","i","e"};
                    for (int k=0;k<methods.length;++k) output.print((k==0 ? "" : ",")+(((Boolean)materialType.getMethod(methods[k]).invoke(mat)) ? 1 : 0));
                    output.println(","+(mat==wood ? 1 : 0)+"},");
                }
            } else {
                output.println("/* Tool ID, speed*2 by block, harvest bitset; vanilla iz/gm/uu/ln registry. */");
                for (int id=0;id<=359;++id) if (id==0 || (id>=256 && id<=286 && id!=259 && id!=260) || id==359) {
                    Object item=id==0 ? null : items[id];
                    Object stack=id==0 ? null : stackType.getConstructor(int.class,int.class,int.class).newInstance(id,1,0);
                    long[] harvest=new long[4];
                    output.print("{"+id+",{");
                    for (int b=0;b<97;++b) {
                        float speed=blocks[b]==null || stack==null ? 1 : ((Float)stackType.getMethod("a",blockType).invoke(stack,blocks[b]));
                        output.print((b==0 ? "" : ",")+Math.round(speed*2));
                        if (blocks[b]!=null) {
                            Object mat=field(blocks[b],"bA");
                            boolean ok=((Boolean)materialType.getMethod("i").invoke(mat)) || (item!=null && ((Boolean)itemType.getMethod("a",blockType).invoke(item,blocks[b])));
                            if (ok) harvest[b/32]|=1L<<(b%32);
                        }
                    }
                    output.print("},{");
                    for (int k=0;k<4;++k) output.print((k==0 ? "" : ",")+"UINT32_C(0x"+Long.toHexString(harvest[k])+")");
                    output.println("}},");
                }
            }
            return;
        }
        if (args.length>0 && args[0].equals("--items")) {
            Object[] items=(Object[])Class.forName("gm").getField("c").get(null);
            System.setOut(output);
            output.println("/* ID, maximum stack, maximum damage: vanilla Beta 1.7.3 gm registry. */");
            for (Object item:items) if (item!=null)
                output.println("{"+integer(item,"bf")+","+integer(item,"bg")+","+Class.forName("gm").getMethod("f").invoke(item)+"},");
            return;
        }
        if (args.length>0 && args[0].equals("--hardness")) {
            Class<?> type=Class.forName("uu");
            Object[] blocks=null;
            for (Field f:type.getDeclaredFields()) if (f.getType().isArray() && f.getType().getComponentType()==type) {
                f.setAccessible(true); blocks=(Object[])f.get(null); break;
            }
            System.setOut(output);
            output.println("/* ID, hardness: vanilla Beta 1.7.3 uu registry. */");
            for (Object block:blocks) if (block!=null)
                output.println("["+integer(block,"bn")+"]="+field(block,"bo")+"f,");
            return;
        }
        Class<?> managerClass = Class.forName("hk");
        Object manager = managerClass.getMethod("a").invoke(null);
        List<?> recipes = (List<?>) field(manager, "b");
        System.setOut(output);
        output.println("/* Numeric recipe data read from the vanilla Beta 1.7.3 client (hk/is/tt/iz). */");
        for (Object recipe : recipes) {
            boolean shaped = recipe.getClass().getName().equals("is");
            Object result = field(recipe, shaped ? "e" : "a");
            Object[] inputs = shaped ? (Object[])field(recipe,"d") : ((List<?>)field(recipe,"b")).toArray();
            int width = shaped ? integer(recipe,"b") : inputs.length;
            int height = shaped ? integer(recipe,"c") : 1;
            output.print("{" + width + "," + height + "," + (shaped ? 0 : 1) + ",{" +
                         integer(result,"c") + "," + integer(result,"a") + "," + integer(result,"d") + "},{");
            for (int i=0;i<inputs.length;++i) {
                if (i>0) output.print(",");
                output.print(ingredient(inputs[i]));
            }
            output.println("}},");
        }
        System.err.println("Read " + recipes.size() + " original Beta recipes");
    }
}
