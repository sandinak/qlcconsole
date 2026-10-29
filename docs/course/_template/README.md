# Lesson template

Copy this whole folder to start a new lesson, then fill in each file.

1. Copy the folder into its level: `level-N/lesson-NN/` (for example
   `level-2/lesson-01/`). On GitHub: open each file here, copy its text,
   and in the level folder **Add file → Create new file** with the same
   name.
2. Replace every `N.N`, `Lesson title`, `TODO` and `<...>`.
3. In the front matter (the lines between `---` at the top), set `title`,
   `parent` and `grand_parent` to match the lesson and its level, so the
   website's sidebar finds it. Copy them from a finished lesson, like
   `level-1/lesson-02/`.
4. Add the lesson to its level's `index.md` table.
5. Write student pages in the course voice: respectful, peer-to-peer,
   real theatre vocabulary, no talking down, and no notes about the
   writing itself (see "Style rules" in `../VOLUNTEER.md`).
6. New words go in the handout's word table **and** in `../GLOSSARY.md`
   with the lesson number.
7. Pictures: write `<!-- PICTURE: what it shows -->` where one goes, and
   ask Branson; the course test takes app pictures (see `../VOLUNTEER.md`).

This folder starts with `_`, so the website leaves it out.

| File | For |
|---|---|
| `index.md` | The lesson's front page: one line, the links, one picture |
| `teacher-guide.md` | The teacher: objectives, timing, demo, what to watch for, answers |
| `handout.md` | Students: the steps, each with *You should see* |
| `slides.md` | A slide outline: one heading per slide |
| `exit-ticket.md` | 3–4 questions on paper |
| `at-home.md` | A. on a computer, B. on paper |
| `student.md` | Students' own page (`/class/N-N/`): the handout cut to its steps, the exit ticket, at home. Check it with `../tools/student-pages.py --qr`, which also makes the QR code for the teacher guide |
