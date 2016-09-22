# -----------------------------------------------------------------------
# Copyright (c) 2016 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the License); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# -----------------------------------------------------------------------

import os
import shutil
from time import sleep, strftime
import serial
import re
import sys


class MbedDeviceManager:
    def __init__(self):
        try:
            import mbed_lstools
        except ImportError, e:
            print("Error: Can't import 'mbed_lstools' module: %s"% e)
        mbed = mbed_lstools.create()
        self.mbed_list = mbed.list_mbeds()
        for dev in self.mbed_list:
            dev['Available'] = True
            # part. structure:{'mount_point': 'D:','serial_port': u'COM3','platform_name': 'LPC1768','Available'=True}

    def dump_all_devices(self):
        print("dump Devices:")
        print(self.mbed_list)


    def get_device(self, platform_type):
        found = False
        index = int(-1)
        mount_point = None
        serial_port = None
        print(platform_type)
        for idx,dev in enumerate(self.mbed_list):
            if dev['platform_name'] == platform_type:
                found = True
                if dev['platform_name'] == platform_type and dev['Available']:
                    mount_point = dev['mount_point']
                    serial_port = dev['serial_port']
                    print('Detected %s. Mount point %s Serial port %s index %s.'
                                % (platform_type, mount_point, serial_port, idx))
                    dev['Available'] = False
                    return idx, mount_point, serial_port
        if found:
            print('Device %s is not available already in use.' %platform_type)
        else:
            print('Device %s is not found.' %platform_type)
        return index, mount_point, serial_port

    def free_device(self, index):
        dev = self.mbed_list[index]
        if not dev['Available']:
            dev['Available'] = True
            print('Release Device %s : %s .' % (index, dev['platform_name']))
        else:
            print('Error: Device %s : %s is already free.' % (index, dev['platform_name']), )

mbed_manager = MbedDeviceManager()

class MBED:
    def __init__(self,platform_type):
        self.manager = mbed_manager
        self.platform_type = platform_type
        self.mount_point = None
        self.serial_port = None
        self.index = -1
        self.running = False

    def detect(self):
        self.index, self.mount_point, self.serial_port = self.manager.get_device(self.platform_type)
        if self.index >= 0:
            return True
        else:
            return False

    def generic_mbed_copy_win(self, source, destination):


        from win32com.shell import shell, shellcon
        import pythoncom
        from os.path import abspath, join
        from glob import glob

        for f in glob(join(destination, '*.bin')):
            os.unlink(f)

        src = shell.SHCreateItemFromParsingName(source, None, shell.IID_IShellItem)
        dest_dir = shell.SHCreateItemFromParsingName(
            abspath(destination),
            None,
            shell.IID_IShellItem
        )
        pfo = pythoncom.CoCreateInstance(
            shell.CLSID_FileOperation,
            None,
            pythoncom.CLSCTX_ALL,
            shell.IID_IFileOperation
        )
        pfo.SetOperationFlags(shellcon.FOF_NO_UI)
        pfo.CopyItem(src, dest_dir, None, None)
        pfo.PerformOperations()

        return True

    def install_bin(self,bin_file_name):
        bin_on_mbed = os.path.join(self.mount_point,os.path.basename(bin_file_name))
        print('%s Copying %s --> %s' %(strftime('%H:%M:%S'),bin_file_name,bin_on_mbed))
        if 'win' in sys.platform:
            self.generic_mbed_copy_win(os.path.abspath(bin_file_name), os.path.abspath(self.mount_point))
        else:
            shutil.copyfile(bin_file_name,bin_on_mbed)
        self.wait_for_file_system()
        print('%s Bin installation complete' %strftime('%H:%M:%S'))

    def run(self,serial_capture_file_name,baud=9600,read_timeout=10,stop_on_serial_read_timeout=False):
        if serial_capture_file_name is not None:
            self.stop_on_serial_read_timeout = stop_on_serial_read_timeout
            from multiprocessing import Process, Event, Pipe
            self.event = Event()
            parent_conn, child_conn = Pipe()
            print('Start serial capture process')
            process_args=(self.serial_port,baud,read_timeout,serial_capture_file_name,stop_on_serial_read_timeout,self.event,child_conn)
            self.capture_process = Process(target=capture_serial,args=process_args)
            self.capture_process.start()
            print('Waiting for pipe input from subprocess')
            self.ip,self.port = parent_conn.recv()
            if not self.port or not self.ip :
                return 1

            print('Received IP %s port %s'%(self.ip,self.port))
        else:
            print('Running without serial capture')
            self.ip,self.port = run_no_capture(self.serial_port,baud,read_timeout)
            print('%s IP %s port %s' % (self.platform_type, self.ip, self.port))
        self.running = True

    def end_run(self, delete_binaries=True, release_manager=True):
        print('MBED end_run')
        if self.running:
            if not self.stop_on_serial_read_timeout:
                self.event.set() # the thread does not stop on its own. send stop signal
            print('MBED end_run waiting for subprocess to terminate')
            self.capture_process.join() # wait for completion
            print('MBED end_run subprocess terminated')
            if delete_binaries:
                print('MBED end_run deleting binaries')
                # delete all binaries on MBED
                filelist = [ f for f in os.listdir(self.mount_point) if f.endswith(".bin") ]
                for f in filelist:
                    print('MBED end_run delete %s',f)
                    os.remove(os.path.join(self.mount_point,f))
                self.wait_for_file_system()
                print('MBED end_run binary delete completed')
            self.running = False
        if release_manager:
            self.manager.free_device(self.index)

    def run_and_capture_till_timeout(self,serial_capture_file_name,baud=9600,read_timeout=10,endOfData=None):
        try:
            print('[%s run_and_capture_till_timeout] Start' %strftime('%H:%M:%S'))
            ser = serial.Serial(self.serial_port,baudrate=baud,timeout=read_timeout)
            if ser == None:
                print(' serial.Serial returned None..')
            capture_file = open(serial_capture_file_name,'w')
            read_size = 1000
            cont = True
            print('[%s run_and_capture_till_timeout] Reseting device..' %strftime('%H:%M:%S'))
            c = reset_mbed(ser)
            if c == None:
                cont = False
            else:
                capture_file.write(c)
            print('[%s run_and_capture_till_timeout] capturing to file...' %strftime('%H:%M:%S'))
            while cont:
                c = ser.read(read_size)
                # Look for the end of test data string and terminate if it is found.
                if endOfData != None:
                    endPos = c.find(endOfData)
                # Clip off the termination string and anything afterwards
                    if endPos != -1:
                        c = c[:(endPos + len(endOfData))]
                capture_file.write(c)
                if endPos != -1:
                    break
                if len(c) < read_size:
                    print('[%s run_and_capture_till_timeout] serial read timeout. Stopping subprocess' %strftime('%H:%M:%S'))
                    print ("exit last Buffsize " + str(len(c)) + "<32" )
                    cont = False
            print('[%s run_and_capture_till_timeout] closing capture file' %strftime('%H:%M:%S'))
            capture_file.close()
            print('[%s run_and_capture_till_timeout] closing serial port' %strftime('%H:%M:%S'))
            ser.flushInput()
            ser.flushOutput()
            ser.close()
            print('[%s run_and_capture_till_timeout] done' %strftime('%H:%M:%S'))
            return True
        except serial.SerialException as e:
            print('[run_and_capture_till_timeout] serial exception',e)
            return False
        
    def wait_for_file_system(self):
        #sleep(25) #MBED file system takes some 'settling' time after it is wrirtten to
        sleep(3) #MBED file system takes some 'settling' time after it is wrirtten to

def reset_mbed(ser):
    for loop in range(5):
        # reset called after open port
        # add sleep for port ready (we saw that sometimes read failed...)
        delay = 5
        print('[%s reset_mbed] loop=%d , delay=%d' %(strftime('%H:%M:%S'), loop, delay))
        sleep(delay)
        try:
            ser.sendBreak()
        except Exception:
            # In linux a termios.error is raised in sendBreak and in
            # setBreak. The following setBreak() is needed to release
            # the reset signal on the target mcu.
            try:
                ser.setBreak(False)
            except:
                pass
        c = ser.read(1)
        if len(c) == 1:
            return c
    print ("Error reading from serial port" )
    return None

def is_valid_ip(ip):
    return re.match(r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$",ip)

def capture_serial(port,baud,read_timeout,capture_file_name,stop_on_serial_read_timeout,event,pipe_conn):
    try:
        print('[capture_serial subprocess] starting..')
        ser = serial.Serial(port,baudrate=baud,timeout=read_timeout)
        capture_file = open(capture_file_name,'w')
        read_size = 32
        cont = True
        print('[capture_serial subprocess] Reseting device..')
        c = reset_mbed(ser)
        if c == None:
           print('[capture_serial subprocess] Reseting device failed')

        if c == None:
            cont = False
            print('[capture_serial subprocess] Failed to get IP from device... exiting ')
            pipe_conn.send([None,None]) # send IP and port
            pipe_conn.close()
            capture_file.close()
            ser.flushInput()
            ser.flushOutput()
            ser.close()
            return 
        else:
            # parse the first received string for IP and port
            ip_str = c.split(':')
            if is_valid_ip(ip_str[1]) :
                pipe_conn.send([ip_str[1],ip_str[2]]) # send IP and port
                print('[capture_serial subprocess] Sending IP to main process %s:%s'%(ip_str[1],ip_str[2]))
                print('[capture_serial subprocess] capturing to file...')
            else:
                print('[capture_serial subprocess] No valid IP address')
                pipe_conn.send([None,None]) # send IP and port
                cont = False
            capture_file.write(c)
        pipe_conn.close()

        while cont:
            c = ser.read(read_size)
            capture_file.write(c)
            if stop_on_serial_read_timeout:
                if len(c) < read_size:
                    print('[capture_serial subprocess] serial read timeout. Stopping subprocess')
                    cont = False
            else:
                if event.is_set():
                    print('[capture_serial subprocess] event is set. Stopping subprocess')
                    cont = False

        print('[capture_serial subprocess] closing capture file')
        capture_file.close()
        print('[capture_serial subprocess] closing serial port')
        ser.flushInput()
        ser.flushOutput()
        ser.close()
        print('[capture_serial subprocess] Subprocess exiting')
    except serial.SerialException as e:
        print('[capture_serial subprocess] serial exception',e)

def run_no_capture(port,baud,read_timeout):
    try:
        ser = serial.Serial(port,baudrate=baud,timeout=read_timeout)
        ip = None
        port = None
        c = reset_mbed(ser)
        if c:
            # parse the first received string for IP and port
            ip_str = c.split(':')
            if is_valid_ip(ip_str[1]) != None:
                ip = ip_str[1]
                port = ip_str[2]
        ser.flushInput()
        ser.flushOutput()
        ser.close()
        return ip,port
    except serial.SerialException as e:
        print e


class MBED_DEVICE(MBED):
    def __init__(self,platform_type):
        print('MBED Device:' + platform_type)
        MBED.__init__(self, platform_type)


