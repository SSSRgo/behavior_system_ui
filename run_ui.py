if __name__ == "__main__":
    try:
        from behavior_system.app import main
    except ModuleNotFoundError as exc:
        missing = exc.name or "dependency"
        print(f"Missing Python package: {missing}")
        print("Install dependencies with:")
        print("  pip install -r requirements.txt")
        raise SystemExit(1)

    raise SystemExit(main())
