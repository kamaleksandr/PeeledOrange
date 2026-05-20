import client.EavTask;
import EAV.EntityL;
import EAV.Attribute;
import EAV.AttributeValue;
import EAV.EntityM;
import java.nio.ByteBuffer;
import java.util.LinkedList;

public class DemoClient {
    public static void main(String[] args) throws Exception {
          // Способ 1: готовый PING
        EavTask pingTask = EavTask.createPing();
        System.out.println("PING task created");
        ByteBuffer buffer = pingTask.getRequestData();
        System.out.println("Packet size: " + buffer.capacity());
        
        // Способ 2: чтение атрибута (0,1000) у сущности (1000040,3)
        LinkedList<EntityL> entities = new LinkedList<>();
        EntityL entity = new EntityL(1000040, 3);
        entity.attributes.add(new Attribute(0, 1000, new AttributeValue("test")));
        entities.add(entity);
        
        EavTask readTask = EavTask.create(EavTask.Commands.READ_ATTRIBUTE, entities);
        System.out.println("READ_ATTRIBUTE task created");
        buffer = readTask.getRequestData();
        System.out.println("Packet size: " + buffer.capacity());
        
        EavTask task = EavTask.createFromArray(buffer);
        for (EntityM e : task.entities){
          System.out.println("Entity: (" + e.num + ", " + e.type + ")");
        }
    }
}