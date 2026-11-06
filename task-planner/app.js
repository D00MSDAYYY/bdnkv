'use strict';

// No build tools or server required. All task data stays in this browser.
const STORAGE_KEY = 'poryadok.tasks.v1';
const PROJECTS = { work: 'Работа', personal: 'Личное', learning: 'Обучение' };
const PRIORITIES = { high: 'Высокий', medium: 'Средний', low: 'Низкий' };
const PRIORITY_ORDER = { high: 0, medium: 1, low: 2 };
const $ = (selector) => document.querySelector(selector);
const dateKey = (date) => `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
const parseDate = (key) => new Date(`${key}T12:00:00`);
const shiftDay = (key, offset) => { const day = parseDate(key); day.setDate(day.getDate() + offset); return dateKey(day); };
const today = () => dateKey(new Date());
const makeId = () => globalThis.crypto?.randomUUID?.() ?? `${Date.now()}-${Math.random().toString(36).slice(2)}`;
const formatDate = (key, options = { day: 'numeric', month: 'short' }) => parseDate(key).toLocaleDateString('ru-RU', options);
const isDateKey = (value) => typeof value === 'string' && /^\d{4}-\d{2}-\d{2}$/.test(value) && Number.isFinite(parseDate(value).getTime()) && dateKey(parseDate(value)) === value;

let tasks = [];
let currentView = 'today';
let selectedDate = today();
let weekAnchor = today();
let currentProject = null;
let statusFilter = 'all';
let editingId = null;
let toastTimer;

function demoTasks() {
  const sample = [
    ['Подготовить концепцию нового проекта', 'Собрать идеи и наметить первые шаги', 'work', 'high', 0, false],
    ['Прочитать 20 страниц книги', 'Немного времени для себя', 'personal', 'low', 0, false],
    ['Пройти урок по JavaScript', 'Продолжить с того места, где остановились', 'learning', 'medium', 0, false],
    ['Разобрать почту и входящие', '', 'work', 'medium', 0, true],
    ['Выйти на прогулку', 'Перезагрузиться и подышать свежим воздухом', 'personal', 'low', 0, true],
    ['Составить план на следующую неделю', '', 'work', 'medium', 2, false],
    ['Повторить заметки из курса', '', 'learning', 'low', 1, false],
  ];
  return sample.map(([title, note, project, priority, offset, done], index) => ({ id: makeId(), title, note, project, priority, date: shiftDay(today(), offset), done, createdAt: Date.now() + index }));
}

function validTask(task) {
  return task && typeof task.id === 'string' && typeof task.title === 'string' && task.title.trim().length > 0 && task.title.length <= 200 && typeof task.note === 'string' && task.note.length <= 1000 && Object.hasOwn(PROJECTS, task.project) && Object.hasOwn(PRIORITIES, task.priority) && (task.date === '' || isDateKey(task.date)) && typeof task.done === 'boolean' && Number.isFinite(task.createdAt);
}

function notify(message) {
  clearTimeout(toastTimer);
  $('#toast').textContent = message;
  $('#toast').hidden = false;
  toastTimer = setTimeout(() => { $('#toast').hidden = true; }, 4500);
}

function saveTasks() {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(tasks)); }
  catch { notify('Не удалось сохранить задачи в браузере. Изменения доступны до закрытия страницы.'); }
}

function loadTasks() {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored === null) { tasks = demoTasks(); saveTasks(); return; }
    const parsed = JSON.parse(stored);
    if (!Array.isArray(parsed) || !parsed.every(validTask) || new Set(parsed.map((task) => task.id)).size !== parsed.length) throw new Error('Invalid task data');
    tasks = parsed;
  } catch {
    // Leave existing storage intact when it cannot be read or validated.
    tasks = [];
    notify('Не удалось прочитать сохранённые задачи. Проверьте доступ к хранилищу браузера.');
  }
}

function renderStats() {
  const daily = tasks.filter((task) => task.date === today());
  const done = daily.filter((task) => task.done).length;
  const percent = daily.length ? Math.round(done / daily.length * 100) : 0;
  $('#stat-total').textContent = daily.length;
  $('#stat-done').textContent = done;
  $('#stat-percent').replaceChildren(document.createTextNode(percent), Object.assign(document.createElement('span'), { textContent: '%' }));
  $('#progress-fill').style.width = `${percent}%`;
  $('.progress-track').setAttribute('aria-valuenow', percent);
  $('#progress-message').textContent = percent === 100 ? 'План выполнен. Можно выдохнуть!' : percent > 0 ? 'Вы движетесь в своём ритме' : 'Всё начинается с первого шага';
  $('#today-count').textContent = daily.filter((task) => !task.done).length;
  $('#all-count').textContent = tasks.filter((task) => !task.done).length;
}

function renderWeek() {
  const anchor = parseDate(weekAnchor);
  const monday = shiftDay(weekAnchor, -((anchor.getDay() + 6) % 7));
  const sunday = shiftDay(monday, 6);
  $('#week-label').textContent = `${formatDate(monday, { day: 'numeric', month: 'long' })} — ${formatDate(sunday, { day: 'numeric', month: 'long', year: 'numeric' })}`;
  const fragment = document.createDocumentFragment();
  for (let i = 0; i < 7; i++) {
    const key = shiftDay(monday, i);
    const day = document.createElement('button');
    day.className = `day${currentView === 'today' && selectedDate === key ? ' selected' : ''}${key === today() ? ' current' : ''}`;
    day.setAttribute('aria-label', formatDate(key, { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' }));
    day.setAttribute('aria-pressed', String(currentView === 'today' && selectedDate === key));
    if (key === today()) day.setAttribute('aria-current', 'date');
    const name = Object.assign(document.createElement('span'), { className: 'day-name', textContent: ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'][i] });
    const number = Object.assign(document.createElement('span'), { className: 'day-number', textContent: parseDate(key).getDate() });
    const hasTasks = tasks.some((task) => task.date === key && !task.done);
    const dot = Object.assign(document.createElement('span'), { className: `day-dot${hasTasks ? ' has-tasks' : ''}` });
    dot.setAttribute('aria-hidden', 'true');
    day.append(name, number, dot);
    day.addEventListener('click', () => { selectedDate = key; currentView = 'today'; currentProject = null; statusFilter = 'all'; render(); });
    fragment.append(day);
  }
  $('#week-days').replaceChildren(fragment);
}

function filteredTasks() {
  const query = $('#search').value.trim().toLocaleLowerCase('ru');
  return tasks.filter((task) => {
    if (currentProject && task.project !== currentProject) return false;
    if (currentView === 'today' && task.date !== selectedDate) return false;
    if (currentView === 'upcoming' && (!task.date || task.date <= today() || task.done)) return false;
    if (currentView === 'done' && !task.done) return false;
    if (statusFilter === 'active' && task.done || statusFilter === 'done' && !task.done) return false;
    return !query || `${task.title} ${task.note}`.toLocaleLowerCase('ru').includes(query);
  }).sort((a, b) => {
    if (currentView !== 'done' && a.done !== b.done) return Number(a.done) - Number(b.done);
    if ($('#sort').value === 'priority') return PRIORITY_ORDER[a.priority] - PRIORITY_ORDER[b.priority] || a.createdAt - b.createdAt;
    if ($('#sort').value === 'date') return (a.date || '9999-12-31').localeCompare(b.date || '9999-12-31') || a.createdAt - b.createdAt;
    return a.createdAt - b.createdAt;
  });
}

function taskRow(task) {
  const row = document.createElement('article');
  row.className = `task-row${task.done ? ' completed' : ''}`;
  const check = Object.assign(document.createElement('input'), { type: 'checkbox', className: 'task-check', checked: task.done });
  check.setAttribute('aria-label', `${task.done ? 'Вернуть в работу' : 'Выполнить'}: ${task.title}`);
  check.addEventListener('change', () => {
    task.done = check.checked;
    saveTasks(); render();
    // Keep keyboard focus after the list is rebuilt.
    const replacement = [...document.querySelectorAll('.task-row')].find((item) => item.dataset.id === task.id);
    if (replacement) replacement.querySelector('input').focus();
    else $('#tasks-heading').focus();
  });
  row.dataset.id = task.id;
  const body = Object.assign(document.createElement('div'), { className: 'task-body' });
  const title = Object.assign(document.createElement('button'), { className: 'task-title', textContent: task.title });
  title.setAttribute('aria-label', `Редактировать: ${task.title}`);
  title.addEventListener('click', () => openTask(task));
  const meta = Object.assign(document.createElement('div'), { className: 'task-meta' });
  const project = Object.assign(document.createElement('span'), { className: 'project-tag' });
  project.append(Object.assign(document.createElement('span'), { className: `project-dot ${task.project}` }), document.createTextNode(PROJECTS[task.project]));
  const due = document.createElement('span');
  due.textContent = task.date ? (task.date === today() ? 'Сегодня' : formatDate(task.date, { day: 'numeric', month: 'short', ...(task.date.slice(0, 4) !== today().slice(0, 4) ? { year: 'numeric' } : {}) })) : 'Без срока';
  if (task.date && task.date < today() && !task.done) { due.className = 'due-overdue'; due.textContent += ' · просрочено'; }
  meta.append(project, due);
  if (task.note) meta.append(Object.assign(document.createElement('span'), { className: 'task-note', textContent: task.note }));
  body.append(title, meta);
  const priority = Object.assign(document.createElement('span'), { className: `priority ${task.priority}`, textContent: PRIORITIES[task.priority], title: `Приоритет: ${PRIORITIES[task.priority]}` });
  priority.setAttribute('aria-label', `Приоритет: ${PRIORITIES[task.priority]}`);
  const remove = Object.assign(document.createElement('button'), { className: 'task-delete', textContent: '×' });
  remove.setAttribute('aria-label', `Удалить: ${task.title}`);
  remove.addEventListener('click', () => {
    tasks = tasks.filter((item) => item.id !== task.id);
    saveTasks(); render();
    $('#tasks-heading').focus();
  });
  row.append(check, body, priority, remove);
  return row;
}

function render() {
  const titles = { today: selectedDate === today() ? 'Сегодня' : formatDate(selectedDate, { day: 'numeric', month: 'long' }), upcoming: 'Предстоящие', all: 'Все задачи', done: 'Выполненные' };
  $('#page-title').textContent = currentProject ? PROJECTS[currentProject] : titles[currentView];
  const subtitles = { today: 'Хороший день начинается с простого плана.', upcoming: 'Всё, что впереди. Время подготовиться без спешки.', all: 'Каждая задача на своём месте.', done: 'Посмотрите, сколько уже получилось.' };
  $('#page-subtitle').textContent = currentProject ? 'Соберите важное в одном месте.' : subtitles[currentView];
  $('#tasks-heading').textContent = currentView === 'today' ? 'План на день' : currentView === 'done' ? 'Готовые задачи' : 'Список задач';
  document.querySelectorAll('[data-view]').forEach((button) => { const active = !currentProject && button.dataset.view === currentView; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  document.querySelectorAll('[data-project]').forEach((button) => { const active = button.dataset.project === currentProject; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  document.querySelectorAll('[data-status]').forEach((button) => { const active = button.dataset.status === statusFilter; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  const doneView = currentView === 'done';
  $('[data-status="active"]').hidden = doneView;
  $('[data-status="done"]').hidden = doneView;
  $('[data-status="done"]').disabled = currentView === 'upcoming';
  renderStats(); renderWeek();
  const visible = filteredTasks();
  $('#list-count').textContent = visible.length;
  const list = $('#task-list');
  list.replaceChildren(...visible.map(taskRow));
  if (!visible.length) {
    const empty = Object.assign(document.createElement('div'), { className: 'empty' });
    empty.append(Object.assign(document.createElement('span'), { className: 'empty-icon', textContent: '✳' }), Object.assign(document.createElement('h3'), { textContent: $('#search').value.trim() ? 'Ничего не нашлось' : 'Здесь пока спокойно' }), Object.assign(document.createElement('p'), { textContent: $('#search').value.trim() ? 'Попробуйте другой запрос или откройте все задачи.' : 'Добавьте новую задачу или выберите другой день.' }));
    list.append(empty);
  }
  $('#topbar-date').textContent = new Date().toLocaleDateString('ru-RU', { day: 'numeric', month: 'long', year: 'numeric' });
}

function openTask(task = null) {
  editingId = task?.id ?? null;
  $('#task-form').reset();
  $('#dialog-title').textContent = task ? 'Редактировать задачу' : 'Новая задача';
  $('#save-task').textContent = task ? 'Сохранить изменения' : 'Создать задачу';
  $('#task-title').value = task?.title ?? '';
  $('#task-title').setCustomValidity('');
  $('#task-note').value = task?.note ?? '';
  $('#task-date').value = task?.date ?? (currentView === 'today' ? selectedDate : currentView === 'upcoming' ? shiftDay(today(), 1) : today());
  $('#task-priority').value = task?.priority ?? 'medium';
  $('#task-project').value = task?.project ?? currentProject ?? 'work';
  $('#task-dialog').showModal();
  $('#task-title').focus();
}

$('#add-task').addEventListener('click', () => openTask());
$('#inline-add').addEventListener('click', () => openTask());
$('#close-dialog').addEventListener('click', () => $('#task-dialog').close());
$('#cancel-dialog').addEventListener('click', () => $('#task-dialog').close());
$('#task-title').addEventListener('input', () => $('#task-title').setCustomValidity(''));
$('#task-form').addEventListener('submit', (event) => {
  event.preventDefault();
  const title = $('#task-title').value.trim();
  if (!title) { $('#task-title').setCustomValidity('Введите название задачи'); $('#task-title').reportValidity(); return; }
  const existing = tasks.find((task) => task.id === editingId);
  const task = { id: existing?.id ?? makeId(), title, note: $('#task-note').value.trim(), date: $('#task-date').value, project: $('#task-project').value, priority: $('#task-priority').value, done: existing?.done ?? false, createdAt: existing?.createdAt ?? Date.now() };
  if (!validTask(task)) { notify('Проверьте название и дату задачи.'); return; }
  if (existing) tasks = tasks.map((item) => item.id === editingId ? task : item);
  else tasks.push(task);
  // Navigate to the saved task so it never disappears behind a filter.
  currentView = task.date ? 'today' : 'all';
  selectedDate = task.date || today();
  weekAnchor = selectedDate;
  currentProject = null; statusFilter = 'all'; $('#search').value = '';
  $('#task-dialog').close();
  saveTasks(); render();
});
document.querySelectorAll('[data-view]').forEach((button) => button.addEventListener('click', () => {
  currentView = button.dataset.view; currentProject = null; statusFilter = 'all';
  $('#search').value = '';
  if (currentView === 'today') { selectedDate = today(); weekAnchor = today(); }
  render();
}));
document.querySelectorAll('[data-project]').forEach((button) => button.addEventListener('click', () => {
  currentProject = button.dataset.project; currentView = 'all'; statusFilter = 'all'; $('#search').value = ''; render();
}));
document.querySelectorAll('[data-status]').forEach((button) => button.addEventListener('click', () => { statusFilter = button.dataset.status; render(); }));
$('#previous-week').addEventListener('click', () => { weekAnchor = shiftDay(weekAnchor, -7); renderWeek(); });
$('#next-week').addEventListener('click', () => { weekAnchor = shiftDay(weekAnchor, 7); renderWeek(); });
$('#reset-week').addEventListener('click', () => { selectedDate = today(); weekAnchor = today(); currentView = 'today'; currentProject = null; statusFilter = 'all'; render(); });
$('#search').addEventListener('input', render);
$('#sort').addEventListener('change', render);
$('#tasks-heading').tabIndex = -1;
document.addEventListener('visibilitychange', () => { if (!document.hidden) render(); });
window.addEventListener('storage', (event) => { if (event.key === STORAGE_KEY) { loadTasks(); render(); } });
loadTasks();
render();
