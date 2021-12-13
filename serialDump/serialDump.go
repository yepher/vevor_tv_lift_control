package main

import (
	"fmt"
	"log"

	"github.com/tarm/serial"
)

func main() {
	// https://github.com/tarm/serial/blob/master/serial.go
	c := &serial.Config{
		Name: "/dev/tty.usbserial-AM00FHIL",
		Baud: 9600,
		//Parity: serial.ParityOdd,
		//StopBits: serial.Stop1,
		Size: 8,
		//ReadTimeout: time.Millisecond * 500,
	}
	s, err := serial.OpenPort(c)
	if err != nil {
		log.Fatal(err)
	}
	
	for {
		buf := make([]byte, 128)
		n, err := s.Read(buf)
		if err != nil {
			log.Fatal(err)
		}

		//fmt.Printf("\n(%x): ", n)

		for i := 0; i < n; i++ {
			if buf[i] == 0x55 {
				fmt.Printf("\n")
			}
			fmt.Printf(" 0x%.2x", buf[i])
		}

	}
}
