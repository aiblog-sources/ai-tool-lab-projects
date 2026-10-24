export class Client {
	#count = 0;

	constructor(public base: string) {}

	// public method
	async get(path: string): Promise<string> {
		return this.#send(path);
	}

	private async helper(path: string): Promise<string> {
		return path;
	}

	// hash-private method
	async #send(path: string): Promise<string> {
		this.#count++;
		return this.base + path;
	}

	static #make(): Client {
		return new Client('');
	}
}
