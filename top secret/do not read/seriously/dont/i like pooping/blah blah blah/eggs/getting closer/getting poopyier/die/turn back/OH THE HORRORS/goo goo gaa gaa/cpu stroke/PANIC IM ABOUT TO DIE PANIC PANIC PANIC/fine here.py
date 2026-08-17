import tkinter as tk
from tkinter import ttk, colorchooser, filedialog, messagebox, simpledialog
from datetime import datetime


class FakeIRCApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Fake IRC Chat")
        self.root.geometry("920x620")
        self.root.minsize(760, 500)

        self.users = {}
        self.messages = []

        self._build_ui()

        self.add_user("Redux", "#ff4040")
        self.add_user("Kernel", "#55aaff")
        self.add_user("CPU", "#ffaa33")

    def _build_ui(self):
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        left = ttk.Frame(main)
        left.pack(side="left", fill="y", padx=(0, 8))

        ttk.Label(left, text="Users").pack(anchor="w")

        self.user_list = tk.Listbox(left, width=22, height=20)
        self.user_list.pack(fill="y", expand=True, pady=(4, 6))
        self.user_list.bind("<<ListboxSelect>>", self._on_user_select)

        user_buttons = ttk.Frame(left)
        user_buttons.pack(fill="x")

        ttk.Button(user_buttons, text="Add User", command=self.prompt_add_user).pack(fill="x", pady=2)
        ttk.Button(user_buttons, text="Change Color", command=self.change_user_color).pack(fill="x", pady=2)
        ttk.Button(user_buttons, text="Remove User", command=self.remove_user).pack(fill="x", pady=2)

        right = ttk.Frame(main)
        right.pack(side="left", fill="both", expand=True)

        topbar = ttk.Frame(right)
        topbar.pack(fill="x", pady=(0, 6))

        self.current_user_label = ttk.Label(topbar, text="Current user: none")
        self.current_user_label.pack(side="left")

        ttk.Button(topbar, text="Export TXT", command=self.export_txt).pack(side="right", padx=2)
        ttk.Button(topbar, text="Export mIRC Log", command=self.export_mirc).pack(side="right", padx=2)
        ttk.Button(topbar, text="Clear Chat", command=self.clear_chat).pack(side="right", padx=2)

        chat_frame = ttk.Frame(right)
        chat_frame.pack(fill="both", expand=True)

        self.chat = tk.Text(
            chat_frame,
            wrap="word",
            state="disabled",
            background="#101010",
            foreground="#e6e6e6",
            insertbackground="white"
        )
        self.chat.pack(side="left", fill="both", expand=True)

        scrollbar = ttk.Scrollbar(chat_frame, orient="vertical", command=self.chat.yview)
        scrollbar.pack(side="right", fill="y")
        self.chat.configure(yscrollcommand=scrollbar.set)

        input_frame = ttk.Frame(right)
        input_frame.pack(fill="x", pady=(8, 0))

        self.message_entry = ttk.Entry(input_frame)
        self.message_entry.pack(side="left", fill="x", expand=True)
        self.message_entry.bind("<Return>", self.send_message_event)

        ttk.Button(input_frame, text="Send", command=self.send_message).pack(side="left", padx=(6, 0))

        system_frame = ttk.Frame(right)
        system_frame.pack(fill="x", pady=(6, 0))

        ttk.Button(system_frame, text="Join Message", command=self.add_join_message).pack(side="left", padx=(0, 4))
        ttk.Button(system_frame, text="Leave Message", command=self.add_leave_message).pack(side="left", padx=4)
        ttk.Button(system_frame, text="Action (/me)", command=self.send_action).pack(side="left", padx=4)

    def add_user(self, name, color=None):
        name = name.strip()
        if not name:
            return
        if name in self.users:
            messagebox.showwarning("User exists", f'"{name}" already exists.')
            return
        if color is None:
            color = "#ffffff"

        self.users[name] = color
        self.user_list.insert("end", name)
        self.chat.tag_configure(self._tag_for_user(name), foreground=color)

        if self.user_list.size() == 1:
            self.user_list.selection_set(0)
            self._on_user_select()

    def prompt_add_user(self):
        name = simpledialog.askstring("Add User", "Nickname:")
        if not name:
            return
        color = colorchooser.askcolor(title="Choose nickname color")[1] or "#ffffff"
        self.add_user(name, color)

    def remove_user(self):
        selection = self.user_list.curselection()
        if not selection:
            return

        index = selection[0]
        name = self.user_list.get(index)
        del self.users[name]
        self.user_list.delete(index)

        if self.user_list.size() > 0:
            new_index = min(index, self.user_list.size() - 1)
            self.user_list.selection_set(new_index)
            self._on_user_select()
        else:
            self.current_user_label.config(text="Current user: none")

    def change_user_color(self):
        user = self.get_selected_user()
        if user is None:
            return

        color = colorchooser.askcolor(
            initialcolor=self.users[user],
            title=f"Color for {user}"
        )[1]
        if color is None:
            return

        self.users[user] = color
        self.chat.tag_configure(self._tag_for_user(user), foreground=color)
        self.redraw_chat()

    def get_selected_user(self):
        selection = self.user_list.curselection()
        if not selection:
            return None
        return self.user_list.get(selection[0])

    def _on_user_select(self, event=None):
        user = self.get_selected_user()
        self.current_user_label.config(
            text=f"Current user: {user}" if user else "Current user: none"
        )

    def send_message_event(self, event):
        self.send_message()
        return "break"

    def send_message(self):
        user = self.get_selected_user()
        if user is None:
            messagebox.showwarning("No user", "Add or select a user first.")
            return

        text = self.message_entry.get().strip()
        if not text:
            return

        message = {
            "type": "message",
            "time": datetime.now(),
            "user": user,
            "text": text
        }
        self.messages.append(message)
        self.message_entry.delete(0, "end")
        self.append_message(message)

    def send_action(self):
        user = self.get_selected_user()
        if user is None:
            return

        text = self.message_entry.get().strip()
        if not text:
            text = simpledialog.askstring("Action", f"What does {user} do?")
        if not text:
            return

        message = {
            "type": "action",
            "time": datetime.now(),
            "user": user,
            "text": text
        }
        self.messages.append(message)
        self.message_entry.delete(0, "end")
        self.append_message(message)

    def add_join_message(self):
        user = self.get_selected_user()
        if user is None:
            return
        message = {"type": "join", "time": datetime.now(), "user": user, "text": ""}
        self.messages.append(message)
        self.append_message(message)

    def add_leave_message(self):
        user = self.get_selected_user()
        if user is None:
            return
        message = {"type": "leave", "time": datetime.now(), "user": user, "text": ""}
        self.messages.append(message)
        self.append_message(message)

    def append_message(self, message):
        self.chat.configure(state="normal")

        timestamp = message["time"].strftime("%H:%M:%S")
        user = message["user"]
        tag = self._tag_for_user(user)

        self.chat.insert("end", f"[{timestamp}] ", "timestamp")

        if message["type"] == "message":
            self.chat.insert("end", f"<{user}> ", tag)
            self.chat.insert("end", message["text"] + "\n")
        elif message["type"] == "action":
            self.chat.insert("end", "* ", "system")
            self.chat.insert("end", user + " ", tag)
            self.chat.insert("end", message["text"] + "\n", "system")
        elif message["type"] == "join":
            self.chat.insert("end", f"*** {user} has joined #redux\n", "system")
        elif message["type"] == "leave":
            self.chat.insert("end", f"*** {user} has left #redux\n", "system")

        self.chat.tag_configure("timestamp", foreground="#888888")
        self.chat.tag_configure("system", foreground="#aaaaaa")

        self.chat.configure(state="disabled")
        self.chat.see("end")

    def redraw_chat(self):
        self.chat.configure(state="normal")
        self.chat.delete("1.0", "end")
        self.chat.configure(state="disabled")
        for message in self.messages:
            self.append_message(message)

    def clear_chat(self):
        if not self.messages:
            return
        if not messagebox.askyesno("Clear Chat", "Clear the entire fake IRC log?"):
            return

        self.messages.clear()
        self.chat.configure(state="normal")
        self.chat.delete("1.0", "end")
        self.chat.configure(state="disabled")

    def export_txt(self):
        if not self.messages:
            messagebox.showinfo("Nothing to export", "There are no messages yet.")
            return

        path = filedialog.asksaveasfilename(
            title="Export TXT",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if not path:
            return

        with open(path, "w", encoding="utf-8") as file:
            for message in self.messages:
                file.write(self.format_plain_message(message))

        messagebox.showinfo("Exported", f"Saved:\n{path}")

    def export_mirc(self):
        if not self.messages:
            messagebox.showinfo("Nothing to export", "There are no messages yet.")
            return

        path = filedialog.asksaveasfilename(
            title="Export mIRC-style log",
            defaultextension=".log",
            filetypes=[
                ("IRC log files", "*.log"),
                ("Text files", "*.txt"),
                ("All files", "*.*")
            ]
        )
        if not path:
            return

        with open(path, "w", encoding="utf-8") as file:
            file.write(f"Session Start: {datetime.now().strftime('%a %b %d %H:%M:%S %Y')}\n")
            file.write("Session Ident: #redux\n")
            file.write("Fake IRC log generated by FakeIRCApp\n\n")

            for message in self.messages:
                file.write(self.format_mirc_message(message))

            file.write("\n")
            file.write(f"Session Close: {datetime.now().strftime('%a %b %d %H:%M:%S %Y')}\n")

        messagebox.showinfo("Exported", f"Saved mIRC-style log:\n{path}")

    @staticmethod
    def format_plain_message(message):
        timestamp = message["time"].strftime("%H:%M:%S")
        user = message["user"]

        if message["type"] == "message":
            return f"[{timestamp}] <{user}> {message['text']}\n"
        if message["type"] == "action":
            return f"[{timestamp}] * {user} {message['text']}\n"
        if message["type"] == "join":
            return f"[{timestamp}] *** {user} has joined #redux\n"
        if message["type"] == "leave":
            return f"[{timestamp}] *** {user} has left #redux\n"
        return ""

    @staticmethod
    def format_mirc_message(message):
        timestamp = message["time"].strftime("%H:%M")
        user = message["user"]

        if message["type"] == "message":
            return f"[{timestamp}] <{user}> {message['text']}\n"
        if message["type"] == "action":
            return f"[{timestamp}] * {user} {message['text']}\n"
        if message["type"] == "join":
            return f"[{timestamp}] * {user} has joined #redux\n"
        if message["type"] == "leave":
            return f"[{timestamp}] * {user} has left #redux\n"
        return ""

    @staticmethod
    def _tag_for_user(name):
        safe = "".join(c if c.isalnum() else "_" for c in name)
        return "user_" + safe


def main():
    root = tk.Tk()
    FakeIRCApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
