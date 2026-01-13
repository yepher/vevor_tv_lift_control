// To Run: go run serialParser.go <filename>
// Example: go run serialParser.go ../raw_serial_captures/encoder_at_bottom.txt

package main

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: %s <filename>\n", os.Args[0])
		os.Exit(1)
	}

	filename := os.Args[1]
	file, err := os.Open(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error opening file: %v\n", err)
		os.Exit(1)
	}
	defer file.Close()

	// Output markdown header
	fmt.Println("# Serial Frame Analysis")
	fmt.Println()
	fmt.Printf("**File:** `%s`\n\n", filename)
	fmt.Println("## Frames")
	fmt.Println()

	scanner := bufio.NewScanner(file)
	lineNum := 0
	for scanner.Scan() {
		lineNum++
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}

		// Parse hex bytes from line like "0x55 0xaa 0xa5 0x8c 0x0c 0x00"
		bytes, err := parseHexLine(line)
		if err != nil {
			fmt.Printf("**Line %d:** Parse error: %v\n\n", lineNum, err)
			continue
		}

		if len(bytes) == 0 {
			continue
		}

		// Output frame as markdown
		fmt.Printf("### Frame %d\n\n", lineNum)
		fmt.Printf("**Raw:** `%s`\n\n", formatBytes(bytes))
		fmt.Printf("**Decoded:** %s\n\n", decodeFrame(bytes))
		fmt.Println()
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file: %v\n", err)
		os.Exit(1)
	}
}

func parseHexLine(line string) ([]byte, error) {
	parts := strings.Fields(line)
	var bytes []byte
	for _, part := range parts {
		if strings.HasPrefix(part, "0x") || strings.HasPrefix(part, "0X") {
			val, err := strconv.ParseUint(part[2:], 16, 8)
			if err != nil {
				return nil, fmt.Errorf("invalid hex byte %s: %v", part, err)
			}
			bytes = append(bytes, byte(val))
		}
	}
	return bytes, nil
}

func formatBytes(bytes []byte) string {
	var parts []string
	for _, b := range bytes {
		parts = append(parts, fmt.Sprintf("0x%02x", b))
	}
	return strings.Join(parts, " ")
}

func decodeFrame(bytes []byte) string {
	if len(bytes) == 0 {
		return "Empty frame"
	}

	// Check for sync bytes
	if bytes[0] != 0x55 {
		return fmt.Sprintf("Invalid: missing sync byte (got 0x%02x)", bytes[0])
	}

	// 2-byte frame: Display sleep or incomplete frame
	if len(bytes) == 2 {
		if bytes[0] == 0x55 && bytes[1] == 0xFC {
			return "**Remote→Lift:** Display sleep command (display turns off)"
		}
		if bytes[0] == 0x55 && bytes[1] == 0xAA {
			return "Incomplete frame (sync bytes only)"
		}
		return fmt.Sprintf("Unknown 2-byte frame")
	}

	if len(bytes) < 2 || bytes[1] != 0xAA {
		return fmt.Sprintf("Invalid: missing 0xAA sync byte (got 0x%02x)", bytes[1])
	}

	// 5-byte frame: Remote→Lift button command
	if len(bytes) == 5 {
		return decodeButtonCommand(bytes)
	}

	// 6-byte frame: Lift→Remote status frame
	if len(bytes) == 6 {
		return decodeStatusFrame(bytes)
	}

	// Incomplete or unknown frame
	if len(bytes) < 5 {
		return fmt.Sprintf("Incomplete frame (%d bytes)", len(bytes))
	}

	return fmt.Sprintf("Unknown frame format (%d bytes)", len(bytes))
}

func decodeButtonCommand(bytes []byte) string {
	if len(bytes) != 5 {
		return "Invalid button command length"
	}

	opcode := bytes[2]
	if bytes[2] == bytes[3] && bytes[3] == bytes[4] {
		// Opcode is repeated 3×
		switch opcode {
		case 0xF0:
			return "**Remote→Lift:** Wake/priming command (sent when remote has been idle)"
		case 0xD1:
			return "**Remote→Lift:** Button 1 press"
		case 0xD2:
			return "**Remote→Lift:** Button 2 press"
		case 0xD3:
			return "**Remote→Lift:** Button 3 press"
		case 0xD7:
			return "**Remote→Lift:** Button 4 press"
		case 0xE1:
			return "**Remote→Lift:** Up button release"
		case 0xE2:
			return "**Remote→Lift:** Down button press"
		case 0xE3:
			return "**Remote→Lift:** Up button press / Down button release"
		default:
			return fmt.Sprintf("**Remote→Lift:** Unknown button command (opcode: 0x%02x)", opcode)
		}
	} else {
		return fmt.Sprintf("**Remote→Lift:** Invalid button command (opcode bytes don't match: 0x%02x 0x%02x 0x%02x)",
			bytes[2], bytes[3], bytes[4])
	}
}

func decodeStatusFrame(bytes []byte) string {
	if len(bytes) != 6 {
		return "Invalid status frame length"
	}

	msgType := bytes[2]
	posLow := bytes[3]
	posHigh := bytes[4]
	status := bytes[5]

	// Decode position (little-endian)
	positionRaw := uint16(posLow) | (uint16(posHigh) << 8)
	positionCm := float64(positionRaw) / 44.0

	// Decode status
	statusDesc := "stopped"
	if status == 0x04 {
		statusDesc = "moving"
	} else if status != 0x00 {
		statusDesc = fmt.Sprintf("unknown status (0x%02x)", status)
	}

	msgTypeDesc := "unknown"
	if msgType == 0xA5 {
		msgTypeDesc = "position frame (A5)"
	} else if msgType == 0xA6 {
		msgTypeDesc = "position frame (A6)"
	} else {
		msgTypeDesc = fmt.Sprintf("unknown type (0x%02x)", msgType)
	}

	return fmt.Sprintf("**Lift→Remote:** Status frame - %s, Position: %.1f cm (raw: 0x%04X = %d), Status: %s",
		msgTypeDesc, positionCm, positionRaw, positionRaw, statusDesc)
}
