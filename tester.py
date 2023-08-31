import usb
import time
import argparse

def main():
    parser = argparse.ArgumentParser(description="USB communication script")
    parser.add_argument("-ctrl", "--control_msg", action="store_true", help="Send control messages")
    parser.add_argument("-blk", "--bulk_msg", action="store_true", help="Send bulk messages")
    parser.add_argument("-text", "--text_msg", type=str, help="Text to send in control messages")
    args = parser.parse_args()

    if not args.control_msg and not args.bulk_msg and not args.text_msg:
        parser.print_help()
        return

    devs = usb.core.find(find_all=True)
    found = False
    for dev in devs:
        if dev.idVendor == 0x2312 and dev.idProduct == 0xec40:
            print("Device found!")
            found = True
            break
    if not found:
        raise Exception("Device not found!")

    dev.set_configuration()
    if args.control_msg:
        send_control_messages(dev)
    if args.bulk_msg:
        send_bulk_messages(dev)
    if args.text_msg:
        send_control_messages_with_text(dev, args.text_msg)
    
    dev.reset()

def send_control_messages(dev):
    for _ in range(3):
        dev.ctrl_transfer(0x40, 0x5b, 1, 0, 0)
        time.sleep(1)
        dev.ctrl_transfer(0x40, 0x5b, 0, 0, 0)
        time.sleep(1)

def send_control_messages_with_text(dev, text):
        print(text.encode())
        dev.ctrl_transfer(0x40, 0x5b, 0, 0, text.encode())
        time.sleep(1)
        w = dev.ctrl_transfer(0xc0, 0x5c, 0, 0, 64)        
        print(bytes(w).decode('utf-8'))

def send_bulk_messages(dev):
    data_to_send = [0, 1]  # A list containing 0 and 1
    ep0 = dev[0][(0,0)][0]
    ep1 = dev[0][(0,0)][1]
    
    for data in data_to_send:
        ep0.write([data], timeout=1000)
        time.sleep(1)

    ep1.clear_halt()
    w = ep1.read(64, 1000)
    print(w)
    print(bytes(w).decode('utf-8'))

if __name__ == "__main__":
    main()
