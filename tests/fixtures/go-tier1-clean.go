package demo

import "os/exec"

func run() {
	exec.Command("echo", "safe")
}
