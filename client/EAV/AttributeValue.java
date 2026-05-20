package EAV;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * Implementation of the AttributeValue from EAV model. The supported parameter
 * types are Byte, Short, Integer, Long, Float, Double, or String
 *
 * @author kamyshev.a
 */
public final class AttributeValue {

  public enum ValueType {
    i8, i16, i32, i64, f4, f8, s, b;
  };
  private ValueType value_type;
  private ByteBuffer value;

  /**
   * @param <T> Byte, Short, Integer, Long, Float, Double or String only.
   * @param val Value to initialize
   * @throws java.lang.Exception
   */
  public <T> AttributeValue(T val) throws Exception {
    setValue(val);
  }

  private AttributeValue() {
  }

  @Override
  public String toString() {
    return getValue().toString();
  }

  public static AttributeValue
          fromString(String s, ValueType vt) throws Exception {
    switch (vt) {
      case i8 -> {
        return new AttributeValue(Byte.valueOf(s));
      }
      case i16 -> {
        return new AttributeValue(Short.valueOf(s));
      }
      case i32 -> {
        return new AttributeValue(Integer.valueOf(s));
      }
      case i64 -> {
        return new AttributeValue(Long.valueOf(s, 16));
      }
      case f4 -> {
        return new AttributeValue(Float.valueOf(s));
      }
      case f8 -> {
        return new AttributeValue(Double.valueOf(s));
      }
      case s -> {
        return new AttributeValue(s);
      }
      case b -> {
        ByteBuffer bb = ByteBuffer.allocate(Byte.parseByte(s));
        return new AttributeValue(bb);
      }
    }
    return new AttributeValue(0);
  }

  public AttributeValue duplicate() {
    AttributeValue av = new AttributeValue();
    av.value_type = value_type;
    av.value = value.duplicate();
    return av;
  }

  /**
   * @return Type of AttributeValue Value
   */
  public ValueType getValueType() {
    return value_type;
  }

  /**
   * @return AttributeValue Value as ByteBuffer
   */
  public ByteBuffer getValueAsBuffer() {
    return value;
  }

  /**
   * @param <T> Can be Byte, Short, Integer, Long, Float, Double or String.
   * @return AttributeValue Value
   */
  @SuppressWarnings("unchecked")
  public <T> T getValue() {
    switch (value_type) {
      case i8 -> {
        return (T) (Byte) value.get(0);
      }
      case i16 -> {
        return (T) (Short) value.getShort(0);
      }
      case i32 -> {
        return (T) (Integer) value.getInt(0);
      }
      case i64 -> {
        return (T) (Long) value.getLong(0);
      }
      case f4 -> {
        return (T) (Float) value.getFloat(0);
      }
      case f8 -> {
        return (T) (Double) value.getDouble(0);
      }
      case s -> {
        String s = new String(value.array(), 0, value.capacity(), StandardCharsets.UTF_8);
        return (T) s;
      }
      case b -> {
        return (T) value;
      }
    }
    return (T) (Integer) 0;
  }

  /**
   * @param <T> Must be Byte, Short, Integer, Long, Float, Double, or String.
   * @param val Value to set.
   * @throws java.lang.Exception
   */
  public <T> void setValue(T val) throws Exception {
    if (val instanceof String string) {
      value_type = ValueType.s;
      byte[] buf = string.getBytes(StandardCharsets.UTF_8);
      value = ByteBuffer.allocate(buf.length);
      value.put(ByteBuffer.wrap(buf));
      return;
    }
    if (val instanceof ByteBuffer byteBuffer) {
      value_type = ValueType.b;
      value = byteBuffer;
      return;
    }
    if (val instanceof Byte aByte) {
      value = ByteBuffer.allocate(1);
      value_type = ValueType.i8;
      value.put(0, aByte);
    } else if (val instanceof Short aShort) {
      value = ByteBuffer.allocate(2);
      value_type = ValueType.i16;
      value.putShort(0, aShort);
    } else if (val instanceof Integer integer) {
      value = ByteBuffer.allocate(4);
      value_type = ValueType.i32;
      value.putInt(0, integer);
    } else if (val instanceof Long aLong) {
      value = ByteBuffer.allocate(8);
      value_type = ValueType.i64;
      value.putLong(0, aLong);
    } else if (val instanceof Float aFloat) {
      value = ByteBuffer.allocate(4);
      value_type = ValueType.f4;
      value.putFloat(0, aFloat);
    } else if (val instanceof Float) {
      value = ByteBuffer.allocate(8);
      value_type = ValueType.f8;
      value.putDouble(0, (Double) val);
    } else {
      value = ByteBuffer.allocate(4);
      value_type = ValueType.i32;
      value.putInt(0, (Integer) 0);
    }
  }

  public int getInt() {
    switch (value_type) {
      case i8 -> {
        return value.get(0);
      }
      case i16 -> {
        return value.getShort(0);
      }
      case i32 -> {
        return value.getInt(0);
      }
      case i64 -> {
        return (int) value.getLong(0);
      }
      default -> {
        return 0;
      }
    }
  }
}
