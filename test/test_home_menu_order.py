"""Check Home menu routing/count/return-focus invariants using production code."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, marker):
    start = source.index(marker)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


enum = function((ROOT / 'src/activities/ActivityManager.h').read_text(), 'enum class HomeMenuItem') + ';'
header = (ROOT / 'src/activities/home/HomeActivity.h').read_text()
reverse = function(header, 'static int menuItemToIndex(')
count = function((ROOT / 'src/activities/home/HomeActivity.cpp').read_text(), 'int HomeActivity::getMenuItemCount()')
harness = r'''
#include <cassert>
#include <vector>
#include <set>
#include "HomeMenuOrder.h"
struct HomeActivity {
    bool deskUi = false;
    std::vector<int> recentBooks;
    HomeMenuItem slot = HomeMenuItem::NONE;
    HomeMenuItem slotItem() const { return slot; }
    int getMenuItemCount() const;
''' + reverse + r'''
};
''' + count + r'''
int main() {
    for (auto slot : {HomeMenuItem::NONE, HomeMenuItem::PLUGINS, HomeMenuItem::OPDS_BROWSER}) {
        HomeActivity home;
        home.slot = slot;
        for (int books : {0, 1, 3, 7}) {
            home.recentBooks.resize(books);
            const int actions = home.getMenuItemCount() - books;
            std::set<HomeMenuItem> found;
            for (int i = 0; i < actions; ++i) {
                const auto item = homeMenuItemAt(i, slot);
                assert(item != HomeMenuItem::NONE);
                assert(found.insert(item).second);
                assert(HomeActivity::menuItemToIndex(item, slot) == i);
            }
            assert(homeMenuItemAt(actions, slot) == HomeMenuItem::NONE);
            assert(found.contains(HomeMenuItem::STATISTICS));
            assert(found.contains(HomeMenuItem::SUDOKU));
            assert(found.contains(HomeMenuItem::SETTINGS_MENU));
            assert(found.contains(HomeMenuItem::PLUGINS) == (slot == HomeMenuItem::PLUGINS));
            assert(found.contains(HomeMenuItem::OPDS_BROWSER) == (slot == HomeMenuItem::OPDS_BROWSER));
            home.deskUi = true;
            assert(home.getMenuItemCount() == books + 1); // Desk keeps one Apps focus stop.
            home.deskUi = false;
        }
    }
}
'''
with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    (temp / 'activities').mkdir()
    (temp / 'activities/ActivityManager.h').write_text('#pragma once\n' + enum)
    (temp / 'menu.cpp').write_text(harness)
    exe = temp / 'menu'
    subprocess.run(['c++', '-std=c++20', '-I', str(temp), '-I', str(ROOT / 'src/activities/home'),
                    str(temp / 'menu.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Home menu invariants pass: counts, dispatch, inverse return focus, optional slots and Desk focus ring')
