import main as pycraft


def main():
    pycraft.log('\n\n', m=1)
    save = pycraft.saves.new_save({'name': 'test', 'seed': 'This is a test!'})
    pycraft.game(pycraft.server_interface.LocalInterface('tester', save, 0))


if __name__ == '__main__':
    main()