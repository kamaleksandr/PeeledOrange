package client;

import EAV.Attribute;
import EAV.AttributeValue;
import EAV.DualKey;
import EAV.EntityL;
import EAV.EntityM;
import common.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.LinkedList;

/**
 *
 * @author kamyshev.a
 */
public class EavTask extends AbstractTask {

  public enum Commands {
    READ_ENTITY,
    READ_ATTRIBUTE,
    WRITE_ATTRIBUTE,
    DELETE_ENTITY,
    DELETE_ATTRIBUTE,
    CREATE_ENTITY,
    READ_HISTORY,
    WRITE_HISTORY,
    READ_FILE,
    WRITE_FILE,
    READ_ALL_DESCRS
  };

  public enum MessageType {
    UNKNOWN,
    CONNECT,
    TASK,
    PING
  }

  private static final byte GET_DESCRIPTORS_FLAG = 1;
  private static final byte GET_PROFILE_FLAG = 2;
  private static final byte GET_CHILDREN_FLAG = 4;

  private static int ID = 1;
  private static final int PACKET_HEADER_SIZE = 4;
  private static final int EAV_HEADER_SIZE = 1;
  public String name;
  public Commands command = Commands.READ_ENTITY;
  public MessageType messageType = MessageType.UNKNOWN;
  public LinkedList<EntityM> entities = new LinkedList<>();
  public int packetID = getIncrementID();
  ByteBuffer requestData = ByteBuffer.allocate(0);

  public EavTask(String name) {
    this.name = name;
  }

  private static int getIncrementID() {
    if ((ID + 1) > 0xFFFF) {
      ID = 1;
    }
    return ID++;
  }

  /**
   * @return the request_data
   */
  public ByteBuffer getRequestData() {
    SetRequested();
    return requestData;
  }

  public void setLoaded() {
    status = StatusEnum.done;
  }

  /*public void setError() {
    status = StatusEnum.error;
  }*/
  private byte bytes(int v) {
    if (v < 0) {
      return 4;
    }
    for (byte i = 0; i < 4; i++) {
      int b = v & (0xFF000000 >> i * 8);
      if (b != 0) {
        return (byte) (4 - i);
      }
    }
    return 0; //return (byte) ((Math.log(v) / Math.log(2)) / 8 + 1);
  }

  private byte[] array(int data) {
    byte[] result = new byte[4];
    result[3] = (byte) ((data & 0xFF000000) >> 24);
    result[2] = (byte) ((data & 0x00FF0000) >> 16);
    result[1] = (byte) ((data & 0x0000FF00) >> 8);
    result[0] = (byte) (data & 0x000000FF);
    return result;
  }

  private static int getInt(byte[] data, int offset, int len) {
    int res = 0;
    for (int i = 0; i < len; i++) {
      res |= ((0x000000ff & data[offset + i]) << i * 8);
    }
    return res;
  }

  private static long getLong(byte[] data, int offset, int len) {
    long res = 0;
    for (int i = 0; i < len; i++) {
      res |= ((0x00000000000000ffL & data[offset + i]) << i * 8);
    }
    return res;
  }

  private ByteBuffer optimizeData(ByteBuffer src) {
    byte[] array = src.array();
    int pos = 0;
    for (; array[pos] == 0; pos++) {
      if (pos + 1 >= array.length) {
        break;
      } // trim the leading zeros
    }
    ByteBuffer dst = ByteBuffer.allocate(array.length - pos);
    for (int i = array.length - 1; i >= pos; i--) {
      dst.put(array[i]);
    }
    return dst;
  }

  private void setRequestData(LinkedList<EntityL> es) {
    byte pid_len = bytes(packetID);
    int ds = 0;
    LinkedList<ByteBuffer> list = new LinkedList<>();
    for (EntityL e : es) {
      ds += EAV_HEADER_SIZE;
      ds += bytes(e.num);
      ds += bytes(e.type);
      for (Attribute a : e.attributes) {
        ds += EAV_HEADER_SIZE;
        ds += bytes(a.num);
        ds += bytes(a.type);
        ByteBuffer bb = a.value.getValueAsBuffer();
        if (a.value.getValueType().ordinal()
                < AttributeValue.ValueType.f4.ordinal()) {
          bb = optimizeData(bb);
        }
        int len = bb.array().length;
        list.add(bb);
        if (len < 256) {
          ds += 2 + len;
        } else {
          ds += 2 + bytes(len) + len;
        }
      }
    }
    byte ds_len = bytes(ds);
    int size = PACKET_HEADER_SIZE + pid_len + ds_len + ds;
    requestData = ByteBuffer.allocate(size);
    //String hex = common.HexConverter.BinToHex(requestData);
    requestData.put(0, (byte) (pid_len | (ds_len << 2)));
    requestData.put(3, (byte) (this.command.ordinal()));
    requestData.position(PACKET_HEADER_SIZE);
    requestData.put(array(packetID), 0, pid_len);
    requestData.put(array(ds), 0, ds_len);
    es.forEach(e -> {
      byte num_len = bytes(e.num);
      byte type_len = bytes(e.type);
      requestData.put((byte) (num_len | (type_len << 3)));
      requestData.put(array(e.num), 0, num_len);
      requestData.put(array(e.type), 0, type_len);
      for (Attribute a : e.attributes) {
        num_len = bytes(a.num);
        type_len = bytes(a.type);
        byte header = (byte) (num_len | (type_len << 3) | (1 << 6)); // 1 - is_attribut  
        ByteBuffer bb = list.poll();
        int len = bb.array().length;
        byte len_len = bytes(len);
        if (len_len == 0) {
          len_len = 1;
        }
        if (len_len < 2) {
          header = (byte) (header | (1 << 7)); // one_byte
        }
        requestData.put(header);
        requestData.put(array(a.num), 0, num_len);
        requestData.put(array(a.type), 0, type_len);
        byte vtype = (byte) a.value.getValueType().ordinal();
        requestData.put(vtype);
        if (len_len > 1) {
          requestData.put(len_len);
        }
        requestData.put(array(len), 0, len_len);
        requestData.put(bb.array(), 0, len);
      }
    });
    int pos = PACKET_HEADER_SIZE + pid_len + ds_len;
    int crc;
    if (ds > 0xFFFF) {
      crc = EgtsCrc.crc16(requestData, pos, 0xFFFF);
    } else {
      crc = EgtsCrc.crc16(requestData, pos, ds);
    }

    requestData.position(1);
    requestData.put(array(crc), 0, 2);
  }

  public static EavTask createObtained(String name) {
    EavTask task = new EavTask(name);
    task.status = StatusEnum.obtained;
    return task;
  }

  public static EavTask createFromArray(ByteBuffer buf) throws Exception {
    byte head = buf.get(0);
    int pid_len = head & 0x03;
    int ds_len = (head & 0x1C) >> 2;
    buf.order(ByteOrder.LITTLE_ENDIAN);
    short crc1 = buf.getShort(1);
    EavTask task = new EavTask("FromArray");
    task.status = StatusEnum.obtained;
    task.command = Commands.values()[buf.get(3)];
    byte data[] = buf.array();
    task.packetID = getInt(data, PACKET_HEADER_SIZE, pid_len);
    int pos = PACKET_HEADER_SIZE + pid_len;
    int ds = getInt(data, pos, ds_len);
    pos += ds_len;

    int crcLen = (ds > 0xFFFF) ? 0xFFFF : ds;
    int crc2 = EgtsCrc.crc16(buf, pos, crcLen);
    if (crc1 != crc2) {
      throw new Exception("EAV task parser, crc does not converge");
    }

   /* int crc2;
    if (ds > 0xFFFF) {
      crc2 = EgtsCrc.crc16(buf, pos, 0xFFFF);
    } else {
      crc2 = EgtsCrc.crc16(buf, pos, ds);
    }
    if (crc1 != crc2) {
      throw (new Exception("EAV task parser, crc does not converge"));
    }*/

    EntityM e = null;
    int size = pos + ds;
    for (; pos + EAV_HEADER_SIZE <= size;) {
      head = buf.get(pos);
      int num_len = head & 0x07;
      int type_len = (head & 0x38) >> 3;
      pos += EAV_HEADER_SIZE;
      if (pos + num_len + type_len > buf.limit()) {
        break;
      }
      int num = getInt(data, pos, num_len);
      pos += num_len;
      int type = getInt(data, pos, type_len);
      pos += type_len;
      if ((head & 0x40) == 0) {
        e = new EntityM(num, type);
        task.entities.add(e);
        continue;
      }
      if (null == e) {
        break;
      }
      if (pos + 2 > buf.limit()) {
        break;
      }
      byte vt = buf.get(pos++);
      if (Integer.compareUnsigned(
              vt, AttributeValue.ValueType.b.ordinal()) > 0) {
        throw (new Exception("Invalid attribute value type"));
      }
      AttributeValue.ValueType vtype = AttributeValue.ValueType.values()[vt];
      int len = Byte.toUnsignedInt(buf.get(pos++));
      if ((head & 0x80) == 0) { // need check!
        if (pos + len > buf.limit()) {
          break;
        }
        int len_len = len;
        len = getInt(data, pos, len_len);
        pos += len_len;
      }
      if (pos + len > buf.limit()) {
        break;
      }
      AttributeValue av;
      switch (vtype) {
        case i8:
          av = new AttributeValue((byte) getInt(data, pos, len));
          break;
        case i16:
          av = new AttributeValue((short) getInt(data, pos, len));
          break;
        case i32:
          av = new AttributeValue(getInt(data, pos, len));
          break;
        case i64:
          av = new AttributeValue(getLong(data, pos, len));
          break;
        case f4:
          av = new AttributeValue(buf.getFloat(pos));
          break;
        case f8:
          av = new AttributeValue(buf.getDouble(pos));
          break;
        case s:
          String s = new String(data, pos, len, StandardCharsets.UTF_8);
          av = new AttributeValue(s);
          break;
        case b:
          ByteBuffer bb = ByteBuffer.allocate(len);
          buf.position(pos);
          buf.get(bb.array(), 0, len);
          av = new AttributeValue(bb);
          break;
        default:
          av = new AttributeValue(0);
      }
      e.attributes.put(new DualKey(num, type), av);
      pos += len;
    }
    return task;
  }

  public static EavTask create(Commands cmd, LinkedList<EntityL> es) {
    EavTask task = new EavTask(null);
    task.command = cmd;
    task.messageType = MessageType.TASK;
    task.setRequestData(es);
    return task;
  }

  public static EavTask createConnect(String clientID, String pass) throws Exception {
    EavTask task = new EavTask("CONNECT");
    task.messageType = MessageType.CONNECT;
    LinkedList<EntityL> es = new LinkedList<>();
    EAV.EntityL e = new EntityL(0, 2);
    es.add(e);
    e.attributes.add(new Attribute(0, 1000, new AttributeValue(clientID)));
    e.attributes.add(new Attribute(0, 1001, new AttributeValue(pass)));
    int flags = GET_DESCRIPTORS_FLAG | GET_PROFILE_FLAG | GET_CHILDREN_FLAG;
    e.attributes.add(new Attribute(0, 1123, new AttributeValue((byte) flags)));
    task.setRequestData(es);
    return task;
  }

  public static EavTask createGetAllDescriptors() throws Exception {
    EavTask task = new EavTask("DESCRIPTORS");
    task.messageType = MessageType.TASK;
    task.command = Commands.READ_ALL_DESCRS;
    task.setRequestData(new LinkedList<>());
    return task;
  }

  public static EavTask createPing() throws Exception {
    EavTask task = new EavTask("PING");
    task.messageType = MessageType.PING;
    task.setRequestData(new LinkedList<>());
    return task;
  }
}
