package main

// =============================================================================
// AuralisOS Universal 4-World Binary Router: auralis-router
// Phase 3 & 4: Zero-Dependency musl Static Binary
// Universal Format Detection & On-Demand Compatibility Execution:
//   1. Windows Portable Executable (.exe, .msi, PE) -> Wine native Wayland driver
//   2. Android Application Package (.apk)           -> Waydroid container runtime
//   3. Linux Distribution Packages (.deb, .rpm)     -> Podman + Distrobox
//   4. Apple macOS Application Bundles (.dmg, .app)  -> Darling translation prefix
// =============================================================================

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
)

const (
	Version = "2.0.0"
	Banner  = "AuralisOS Universal 4-World Binary Router v" + Version
)

// Supported binary worlds
type TargetWorld int

const (
	WorldUnknown TargetWorld = iota
	WorldWindows             // .exe, .msi, PE
	WorldAndroid             // .apk, Dalvik/ART
	WorldDebian              // .deb, dpkg/apt
	WorldRPM                 // .rpm, dnf/rpm
	WorldMacOS               // .dmg, .app, Mach-O
)

func (w TargetWorld) String() string {
	switch w {
	case WorldWindows:
		return "Windows (Wine-Wayland Native)"
	case WorldAndroid:
		return "Android (Waydroid Multi-Window)"
	case WorldDebian:
		return "Linux Debian (Distrobox Container)"
	case WorldRPM:
		return "Linux Fedora (Distrobox Container)"
	case WorldMacOS:
		return "Apple macOS (Darling Mach-O Translation)"
	default:
		return "Native Linux / Unknown"
	}
}

func main() {
	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	targetArg := os.Args[1]

	// Handle flags
	if targetArg == "--help" || targetArg == "-h" {
		printUsage()
		os.Exit(0)
	}

	if targetArg == "--version" || targetArg == "-v" {
		fmt.Println(Banner)
		os.Exit(0)
	}

	if targetArg == "--detect" && len(os.Args) >= 3 {
		filePath := os.Args[2]
		world, reason := detectWorld(filePath)
		fmt.Printf("File: %s\nWorld: %s\nReason: %s\n", filePath, world, reason)
		os.Exit(0)
	}

	if targetArg == "--hydrate" && len(os.Args) >= 3 {
		subsystemId := os.Args[2]
		cmd := exec.Command("auralis-hydrate", "enable", subsystemId)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		_ = cmd.Run()
		os.Exit(0)
	}

	filePath, err := filepath.Abs(targetArg)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Error resolving absolute path: %v\n", err)
		os.Exit(1)
	}

	// Verify file existence
	fileInfo, err := os.Stat(filePath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] File not found: %s\n", filePath)
		os.Exit(1)
	}

	// Forwarded arguments to the application
	passthroughArgs := []string{}
	if len(os.Args) > 2 {
		passthroughArgs = os.Args[2:]
	}

	world, reason := detectWorld(filePath)
	fmt.Printf("[AURALIS-ROUTER] Routing '%s'\n -> World: %s (%s)\n", filepath.Base(filePath), world, reason)

	switch world {
	case WorldWindows:
		routeWindows(filePath, passthroughArgs)
	case WorldAndroid:
		routeAndroid(filePath, passthroughArgs)
	case WorldDebian, WorldRPM:
		routeLinuxPackage(filePath, world, passthroughArgs)
	case WorldMacOS:
		routeMacOS(filePath, fileInfo.IsDir(), passthroughArgs)
	default:
		// Attempt native execution fallback
		fmt.Printf("[AURALIS-ROUTER] Unrecognized format, attempting native execution...\n")
		execNative(filePath, passthroughArgs)
	}
}

func printUsage() {
	fmt.Println(Banner)
	fmt.Println("Usage: auralis-router <path-to-binary-or-package> [arguments...]")
	fmt.Println("       auralis-router --detect <path>")
	fmt.Println("       auralis-router --hydrate <subsystem>")
	fmt.Println("       auralis-router --version")
	fmt.Println("")
	fmt.Println("Supported Formats & Worlds:")
	fmt.Println("  1. Windows (.exe, .msi, PE)         -> Wine native Wayland driver without Xwayland")
	fmt.Println("  2. Android (.apk)                   -> Waydroid OpenRC container & multi-window session")
	fmt.Println("  3. Linux Packages (.deb, .rpm)      -> Podman + Distrobox persistent container export")
	fmt.Println("  4. Apple macOS (.dmg, .app, Mach-O) -> 7z/darling-dmg extraction & Darling translation")
	fmt.Println("")
	fmt.Println("On-Demand Hydration:")
	fmt.Println("  If an engine is not yet installed, auralis-router opens an on-the-spot activation prompt.")
}

// =============================================================================
// Phase 4 On-Demand Subsystem Activation & Modal Prompt Wiring
// =============================================================================

func isSubsystemActive(subsystemId string) bool {
	// 1. Check if corresponding core binary is found in PATH
	binCheck := map[string][]string{
		"wine":      {"wine-wayland", "wine"},
		"waydroid":  {"waydroid"},
		"distrobox": {"distrobox", "podman"},
		"darling":   {"darling"},
	}

	for _, b := range binCheck[subsystemId] {
		if _, err := exec.LookPath(b); err == nil {
			return true
		}
	}

	// 2. Check if mounted in /proc/mounts
	mountPoint := filepath.Join("/var/lib/auralis/mnt", subsystemId)
	mountsData, err := os.ReadFile("/proc/mounts")
	if err == nil && strings.Contains(string(mountsData), mountPoint) {
		return true
	}

	return false
}

func ensureSubsystemActive(subsystemId, subsystemName, filePath string) bool {
	if isSubsystemActive(subsystemId) {
		return true
	}

	fmt.Printf("[AURALIS-ROUTER] Required subsystem '%s' is not yet active for '%s'.\n", subsystemName, filepath.Base(filePath))

	// 1. If running under Wayland/X11 GUI, trigger graphical prompt dialog
	if os.Getenv("WAYLAND_DISPLAY") != "" || os.Getenv("DISPLAY") != "" {
		// Prefer auralis-store modal prompt
		if _, err := exec.LookPath("auralis-store"); err == nil {
			fmt.Println("[AURALIS-ROUTER] Spawning on-demand activation dialog in auralis-store...")
			cmd := exec.Command("auralis-store", "--prompt", subsystemId, filePath)
			_ = cmd.Run()
			return isSubsystemActive(subsystemId)
		} else if _, err := exec.LookPath("zenity"); err == nil {
			promptText := fmt.Sprintf("%s is required to execute '%s'.\n\nWould you like to download and activate this subsystem now?", subsystemName, filepath.Base(filePath))
			cmd := exec.Command("zenity", "--question", "--title=AuralisOS Subsystem Required", "--text="+promptText)
			if err := cmd.Run(); err == nil {
				fmt.Println("[AURALIS-ROUTER] Activating subsystem via auralis-hydrate...")
				hydCmd := exec.Command("auralis-hydrate", "enable", subsystemId)
				hydCmd.Stdout = os.Stdout
				hydCmd.Stderr = os.Stderr
				_ = hydCmd.Run()
				return isSubsystemActive(subsystemId)
			}
			return false
		}
	}

	// 2. Terminal Interactive Prompt Fallback
	fmt.Printf("[AURALIS-ROUTER] Would you like to download and activate %s now? [Y/n]: ", subsystemName)
	reader := bufio.NewReader(os.Stdin)
	ans, _ := reader.ReadString('\n')
	ans = strings.TrimSpace(strings.ToLower(ans))

	if ans == "" || ans == "y" || ans == "yes" {
		fmt.Printf("[AURALIS-ROUTER] Invoking auralis-hydrate enable %s...\n", subsystemId)
		hydCmd := exec.Command("auralis-hydrate", "enable", subsystemId)
		hydCmd.Stdout = os.Stdout
		hydCmd.Stderr = os.Stderr
		if err := hydCmd.Run(); err != nil {
			fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Failed to activate subsystem: %v\n", err)
			return false
		}
		return isSubsystemActive(subsystemId)
	}

	fmt.Println("[AURALIS-ROUTER] Subsystem activation declined by user.")
	return false
}

// -----------------------------------------------------------------------------
// Magic Bytes & Extension Detection Engine
// -----------------------------------------------------------------------------
func detectWorld(filePath string) (TargetWorld, string) {
	ext := strings.ToLower(filepath.Ext(filePath))

	// Direct directory bundle check (.app)
	info, err := os.Stat(filePath)
	if err == nil && info.IsDir() {
		if strings.HasSuffix(strings.ToLower(filePath), ".app") {
			return WorldMacOS, "macOS Application Bundle Directory (.app)"
		}
	}

	// Read initial magic bytes header (up to 512 bytes)
	header := make([]byte, 512)
	file, err := os.Open(filePath)
	if err == nil {
		defer file.Close()
		_, _ = io.ReadFull(file, header)
	}

	// 1. Windows PE / DOS Magic Check: "MZ" at offset 0
	if len(header) >= 2 && header[0] == 'M' && header[1] == 'Z' {
		// Verify PE signature at offset indicated by e_lfanew (offset 0x3C)
		if len(header) >= 0x40 {
			peOffset := binary.LittleEndian.Uint32(header[0x3C:0x40])
			if int(peOffset+4) <= len(header) {
				if header[peOffset] == 'P' && header[peOffset+1] == 'E' && header[peOffset+2] == 0 && header[peOffset+3] == 0 {
					return WorldWindows, "Windows Portable Executable (PE/COFF) Magic"
				}
			}
		}
		return WorldWindows, "MS-DOS / Windows Executable (MZ Magic)"
	}

	// Windows MSI Composite Document File V2 check: D0 CF 11 E0 A1 B1 1A E1
	msiMagic := []byte{0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}
	if len(header) >= 8 && bytes.Equal(header[:8], msiMagic) {
		if ext == ".msi" || ext == ".msp" {
			return WorldWindows, "Microsoft Windows Installer Package (MSI Compound)"
		}
	}

	// 2. Apple macOS Mach-O Magic Bytes Check
	// 64-bit Mach-O (Little Endian): CF FA ED FE
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xCF, 0xFA, 0xED, 0xFE}) {
		return WorldMacOS, "Apple macOS Mach-O 64-bit Binary (Little Endian)"
	}
	// 64-bit Mach-O (Big Endian): FE ED FA CF
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xFE, 0xED, 0xFA, 0xCF}) {
		return WorldMacOS, "Apple macOS Mach-O 64-bit Binary (Big Endian)"
	}
	// 32-bit Mach-O (Little Endian): CE FA ED FE
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xCE, 0xFA, 0xED, 0xFE}) {
		return WorldMacOS, "Apple macOS Mach-O 32-bit Binary (Little Endian)"
	}
	// Universal/Fat Mach-O: CA FE BA BE
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xCA, 0xFE, 0xBA, 0xBE}) {
		// Note: Java bytecode also uses CAFEBABE, disambiguate with extension or Mach header
		if ext != ".class" && ext != ".jar" {
			return WorldMacOS, "Apple macOS Universal/Fat Mach-O Binary"
		}
	}
	// Universal Fat Mach-O (64-bit): CA FE BA BF
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xCA, 0xFE, 0xBA, 0xBF}) {
		return WorldMacOS, "Apple macOS Universal 64-bit Fat Mach-O Binary"
	}

	// Apple Disk Image (.dmg) UDIF header or extension
	if ext == ".dmg" {
		return WorldMacOS, "Apple Disk Image (.dmg Archive)"
	}

	// 3. Android Package (.apk) Check: ZIP format (PK\x03\x04) containing AndroidManifest.xml
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{'P', 'K', 0x03, 0x04}) {
		if ext == ".apk" || bytes.Contains(header, []byte("AndroidManifest.xml")) {
			return WorldAndroid, "Android Application Package (APK / Dalvik-ART)"
		}
	}

	// 4. Linux Package Formats
	// Debian Package: Ar archive signature "!<arch>\ndebian-binary"
	if len(header) >= 8 && string(header[:8]) == "!<arch>\n" {
		if ext == ".deb" || bytes.Contains(header, []byte("debian-binary")) {
			return WorldDebian, "Debian Binary Package (.deb)"
		}
	}

	// Red Hat Package: RPM Lead Magic \xed\xab\xee\xdb
	if len(header) >= 4 && bytes.Equal(header[:4], []byte{0xED, 0xAB, 0xEE, 0xDB}) {
		return WorldRPM, "Red Hat Package Manager Archive (.rpm)"
	}

	// Extension-based fallbacks
	switch ext {
	case ".exe", ".msi", ".bat", ".cmd":
		return WorldWindows, "Windows Extension Fallback"
	case ".apk":
		return WorldAndroid, "Android Extension Fallback"
	case ".deb":
		return WorldDebian, "Debian Extension Fallback"
	case ".rpm":
		return WorldRPM, "RPM Extension Fallback"
	case ".dmg", ".app":
		return WorldMacOS, "macOS Extension Fallback"
	}

	return WorldUnknown, "Native Linux ELF or Unrecognized Script"
}

// -----------------------------------------------------------------------------
// World 1: Windows Runtime (Wine-Wayland Native Driver)
// -----------------------------------------------------------------------------
func routeWindows(filePath string, args []string) {
	// Phase 4 On-Demand Hydration Check
	if !ensureSubsystemActive("wine", "Windows Engine (Wine-Wayland)", filePath) {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Execution aborted: Windows engine not available.\n")
		return
	}

	home := os.Getenv("HOME")
	if home == "" {
		home = "/root"
	}

	winePrefix := filepath.Join(home, ".local", "share", "auralis", "wine")
	os.Setenv("WINEPREFIX", winePrefix)
	os.Setenv("WINEDEBUG", "-all")
	os.Setenv("WINE_WAYLAND_DRIVER", "1")

	// Ensure prefix directory exists
	if err := os.MkdirAll(winePrefix, 0755); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Failed to create Wine prefix: %v\n", err)
	}

	systemReg := filepath.Join(winePrefix, "system.reg")
	if _, err := os.Stat(systemReg); os.IsNotExist(err) {
		fmt.Println("[AURALIS-ROUTER] Initializing on-demand ultra-low-memory Wine prefix...")
		os.Setenv("WINEDLLOVERRIDES", "mscoree,mshtml=")
		cmd := exec.Command("wineboot", "-u")
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		_ = cmd.Run()

		fmt.Println("[AURALIS-ROUTER] Configuring native Wayfire Wayland driver in Wine registry...")
		regCmd := exec.Command("wine", "reg", "add", "HKCU\\Software\\Wine\\Drivers", "/v", "Graphics", "/t", "REG_SZ", "/d", "wayland", "/f")
		_ = regCmd.Run()
	}

	var wineBin string
	if _, err := exec.LookPath("wine-wayland"); err == nil {
		wineBin = "wine-wayland"
	} else if _, err := exec.LookPath("wine"); err == nil {
		wineBin = "wine"
	} else {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Wine executable not found in PATH.\n")
		os.Exit(1)
	}

	var execArgs []string
	if strings.ToLower(filepath.Ext(filePath)) == ".msi" {
		execArgs = append([]string{"msiexec", "/i", filePath}, args...)
	} else {
		execArgs = append([]string{filePath}, args...)
	}

	fmt.Printf("[AURALIS-ROUTER] Spawning native Wayland Windows binary: %s\n", filePath)
	cmd := exec.Command(wineBin, execArgs...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Wine execution exited: %v\n", err)
	}
}

// -----------------------------------------------------------------------------
// World 2: Android Runtime (Waydroid On-Demand Container & Multi-Window)
// -----------------------------------------------------------------------------
func routeAndroid(filePath string, args []string) {
	// Phase 4 On-Demand Hydration Check
	if !ensureSubsystemActive("waydroid", "Android Engine (Waydroid)", filePath) {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Execution aborted: Android engine not available.\n")
		return
	}

	fmt.Println("[AURALIS-ROUTER] Verifying Waydroid OpenRC container service...")
	// Start OpenRC container service on demand if not active
	statusCmd := exec.Command("rc-service", "waydroid-container", "status")
	if err := statusCmd.Run(); err != nil {
		fmt.Println("[AURALIS-ROUTER] Starting waydroid-container service on demand...")
		startCmd := exec.Command("rc-service", "waydroid-container", "start")
		startCmd.Stdout = os.Stdout
		startCmd.Stderr = os.Stderr
		_ = startCmd.Run()
	}

	// Initialize Waydroid multi-window property
	propCmd := exec.Command("waydroid", "prop", "set", "persist.waydroid.multi_windows", "true")
	_ = propCmd.Run()
	fmt.Println("[AURALIS-ROUTER] Enforcing Waydroid multi-window Wayfire integration...")

	// Verify session status
	sessCheck := exec.Command("waydroid", "status")
	sessOut, _ := sessCheck.CombinedOutput()
	if !strings.Contains(string(sessOut), "RUNNING") {
		fmt.Println("[AURALIS-ROUTER] Initializing Waydroid session...")
		sessStart := exec.Command("waydroid", "session", "start")
		if err := sessStart.Start(); err != nil {
			fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Warning: Could not start Waydroid session: %v\n", err)
		}
	}

	fmt.Printf("[AURALIS-ROUTER] Installing Android package: %s\n", filepath.Base(filePath))
	installCmd := exec.Command("waydroid", "app", "install", filePath)
	installCmd.Stdout = os.Stdout
	installCmd.Stderr = os.Stderr
	if err := installCmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] APK installation error: %v\n", err)
		return
	}

	pkgName := extractPackageName(filePath)
	if pkgName != "" {
		fmt.Printf("[AURALIS-ROUTER] Launching Android app: %s\n", pkgName)
		launchCmd := exec.Command("waydroid", "app", "launch", pkgName)
		launchCmd.Stdout = os.Stdout
		launchCmd.Stderr = os.Stderr
		_ = launchCmd.Run()
	} else {
		fmt.Println("[AURALIS-ROUTER] APK installed successfully. Available in auralis-shell launcher.")
	}
}

func extractPackageName(apkPath string) string {
	if _, err := exec.LookPath("aapt"); err == nil {
		out, err := exec.Command("aapt", "dump", "badging", apkPath).Output()
		if err == nil {
			for _, line := range strings.Split(string(out), "\n") {
				if strings.HasPrefix(line, "package: name='") {
					parts := strings.Split(line, "'")
					if len(parts) >= 2 {
						return parts[1]
					}
				}
			}
		}
	}
	base := filepath.Base(apkPath)
	return strings.TrimSuffix(base, filepath.Ext(base))
}

// -----------------------------------------------------------------------------
// World 3: Linux Package Runtime (Podman + Distrobox Rootless Containers)
// -----------------------------------------------------------------------------
func routeLinuxPackage(filePath string, world TargetWorld, args []string) {
	// Phase 4 On-Demand Hydration Check
	if !ensureSubsystemActive("distrobox", "Linux Containers (Distrobox)", filePath) {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Execution aborted: Linux container engine not available.\n")
		return
	}

	var containerName, baseImage, installCmd string
	if world == WorldDebian {
		containerName = "auralis-debian"
		baseImage = "docker.io/library/debian:bookworm-slim"
		installCmd = fmt.Sprintf("sudo dpkg -i %s || sudo apt-get install -f -y", filePath)
	} else {
		containerName = "auralis-fedora"
		baseImage = "docker.io/library/fedora:latest"
		installCmd = fmt.Sprintf("sudo dnf install -y %s", filePath)
	}

	fmt.Printf("[AURALIS-ROUTER] Checking persistent container: %s (%s)...\n", containerName, baseImage)

	listOut, _ := exec.Command("distrobox", "list").CombinedOutput()
	if !strings.Contains(string(listOut), containerName) {
		fmt.Printf("[AURALIS-ROUTER] Initializing Distrobox container '%s'...\n", containerName)
		createCmd := exec.Command("distrobox", "create", "--name", containerName, "--image", baseImage, "--yes")
		createCmd.Stdout = os.Stdout
		createCmd.Stderr = os.Stderr
		if err := createCmd.Run(); err != nil {
			fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Distrobox creation error: %v\n", err)
			return
		}
	}

	fmt.Printf("[AURALIS-ROUTER] Installing package inside %s...\n", containerName)
	installExec := exec.Command("distrobox", "enter", containerName, "--", "sh", "-c", installCmd)
	installExec.Stdout = os.Stdout
	installExec.Stderr = os.Stderr
	if err := installExec.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Package installation failed: %v\n", err)
		return
	}

	appName := strings.TrimSuffix(filepath.Base(filePath), filepath.Ext(filePath))
	fmt.Printf("[AURALIS-ROUTER] Exporting application to auralis-shell Start Menu: %s...\n", appName)
	exportCmd := exec.Command("distrobox", "enter", containerName, "--", "distrobox-export", "--app", appName)
	_ = exportCmd.Run()

	fmt.Println("[AURALIS-ROUTER] Package deployed and integrated into auralis-shell.")
}

// -----------------------------------------------------------------------------
// World 4: Apple macOS Runtime (7z/darling-dmg Extraction & Darling Translation)
// -----------------------------------------------------------------------------
func routeMacOS(filePath string, isDir bool, args []string) {
	// Phase 4 On-Demand Hydration Check
	if !ensureSubsystemActive("darling", "macOS Engine (Darling)", filePath) {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Execution aborted: macOS Darling engine not available.\n")
		return
	}

	home := os.Getenv("HOME")
	if home == "" {
		home = "/root"
	}

	machoPath := ""

	if isDir && strings.HasSuffix(strings.ToLower(filePath), ".app") {
		machoPath = locateMachOInApp(filePath)
	} else if strings.ToLower(filepath.Ext(filePath)) == ".dmg" {
		extractDir := filepath.Join(home, ".local", "share", "auralis", "dmg", strings.TrimSuffix(filepath.Base(filePath), ".dmg"))
		_ = os.MkdirAll(extractDir, 0755)

		fmt.Printf("[AURALIS-ROUTER] Extracting Apple Disk Image (.dmg) to: %s...\n", extractDir)
		if _, err := exec.LookPath("7z"); err == nil {
			cmd := exec.Command("7z", "x", "-y", filePath, "-o"+extractDir)
			_ = cmd.Run()
		} else if _, err := exec.LookPath("darling-dmg"); err == nil {
			cmd := exec.Command("darling-dmg", filePath, extractDir)
			_ = cmd.Run()
		}

		_ = filepath.Walk(extractDir, func(path string, info os.FileInfo, err error) error {
			if err == nil && info.IsDir() && strings.HasSuffix(strings.ToLower(path), ".app") {
				machoPath = locateMachOInApp(path)
				return filepath.SkipDir
			}
			return nil
		})
	} else {
		machoPath = filePath
	}

	if machoPath == "" {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Could not locate Mach-O executable inside bundle: %s\n", filePath)
		return
	}

	fmt.Printf("[AURALIS-ROUTER] Spawning Mach-O binary via Darling translation prefix: %s\n", machoPath)
	darlingCmd := exec.Command("darling", append([]string{machoPath}, args...)...)
	darlingCmd.Stdin = os.Stdin
	darlingCmd.Stdout = os.Stdout
	darlingCmd.Stderr = os.Stderr
	if err := darlingCmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Darling execution error: %v\n", err)
	}
}

func locateMachOInApp(appDir string) string {
	macosDir := filepath.Join(appDir, "Contents", "MacOS")
	entries, err := os.ReadDir(macosDir)
	if err != nil {
		return ""
	}
	for _, entry := range entries {
		if !entry.IsDir() {
			return filepath.Join(macosDir, entry.Name())
		}
	}
	return ""
}

// Native execution fallback
func execNative(filePath string, args []string) {
	allArgs := append([]string{filePath}, args...)
	if err := syscall.Exec(filePath, allArgs, os.Environ()); err != nil {
		fmt.Fprintf(os.Stderr, "[AURALIS-ROUTER] Native execution error: %v\n", err)
		os.Exit(1)
	}
}
