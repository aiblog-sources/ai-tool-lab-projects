export class Client {
	#count = 0;

	constructor(base) {
		this.base = base;
	}

	// public method
	async get(path) {
		return this.#send(path);
	}

	async helper(path) {
		return path;
	}

	// hash-private method
	async #send(path) {
		this.#count++;
		return this.base + path;
	}

	static #make() {
		return new Client('');
	}
}
