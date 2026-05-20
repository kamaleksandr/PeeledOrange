package EAV;

import java.util.*;

/**
 * Implementation of the Entity from EAV model.
 * LinkedList attributes are used
 * @author kamyshev.a
 */
public class EntityL {

    public int num, type;

    public final LinkedList<Attribute> attributes;

    /**
     * @param num Entity number;
     * @param type Entity type;
     */
    public EntityL(int num, int type) {
        this.num = num;
        this.type = type;
        attributes = new LinkedList<>();
    }
    
}
