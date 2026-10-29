package sample

import "fmt"
import text "strings"

type Widget struct {
	Name string
}

var Count int
const Limit = 3

func Add(value int) int {
	local := value
	fmt.Println(local)
	return local
}

func (w *Widget) Save() {
	text.TrimSpace(w.Name)
}
