package demo

import (
	"crypto/md5"
	"crypto/sha1"
	"crypto/tls"
	"os"
	"os/exec"
	"unsafe"
)

type Config struct{}

func run(program string) {
	exec.Command(program)
	_ = os.Remove("temporary")
	md5.Sum([]byte("one"))
	sha1.Sum([]byte("two"))
	unsafe.Pointer(nil)
	cfg := tls.Config{InsecureSkipVerify: true}
	os.Chmod("world", 0777)
}
