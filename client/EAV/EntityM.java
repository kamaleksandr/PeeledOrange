package EAV;

/**
 * Implementation of the Entity from EAV model. TreeMap attributes are used
 *
 * @author kamyshev.a
 */
public class EntityM {

  public int num, type;

  public final AttributesMap attributes;

  /**
   * @param num Entity number;
   * @param type Entity type;
   */
  public EntityM(int num, int type) {
    this.num = num;
    this.type = type;
    attributes = new AttributesMap();
  }

  public static EntityM createFrom(EntityL el) {
    EntityM em = new EntityM(el.num, el.type);
    el.attributes.forEach((a) -> {
      em.attributes.put(new DualKey(a.num, a.type), a.value.duplicate());
    });
    return em;
  }

}
