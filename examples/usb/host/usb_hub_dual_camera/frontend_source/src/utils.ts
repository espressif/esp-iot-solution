export interface Resolution {
  format: string;
  width: number;
  height: number;
  index: number;
}

export interface Camera {
  id: string | number;
  resolutions: Resolution[];
  src?: string | null;
}

export interface CameraActived {
  id: string | number;
  resolution: Resolution;
  src?: URL;
}

export class WebBase {
  public readonly base_url: URL;
  public readonly all_ports: Array<number> = [80, 8080];
  private readonly available_ports: Array<number> = [...this.all_ports];

  constructor(baseUrl: URL) {
    this.base_url = baseUrl;
  }

  getOneAvailablePort = (ro = true): number | null => {
    if (this.available_ports.length === 0) {
      return null;
    }
    if (ro) {
      return this.available_ports[0] || null;
    }
    return this.available_ports.pop() || null;
  };

  pop = (): number | null => {
    return this.getOneAvailablePort(false);
  };

  getAllAvailablePorts = (): Array<number> => {
    return [...this.available_ports];
  };

  getAllAvailablePortsLength = (): number => this.available_ports.length;

  // -> false: failed
  // -> true: success
  releasePort = (port: number | string): boolean => {
    let port_num: number;
    if (typeof port === "string") {
      port_num = Number.parseInt(port);
    } else {
      port_num = port;
    }

    if (this.all_ports.includes(port_num)) {
      if (!this.available_ports.includes(port_num)) {
        this.available_ports.push(port_num);
      }
      return true;
    }

    return false;
  };

  getAvailableUrl(path: string, urlport: string | number | null): URL | null;
  getAvailableUrl(path: string, ro?: boolean): URL | null;

  getAvailableUrl(
    path: string,
    parma1?: string | number | boolean | null
  ): URL | null {
    const url = new URL(this.base_url);
    url.pathname = `/${path}`.replace(/\/\//g, "/");
    url.search = "";

    switch (typeof parma1) {
      case "undefined":
      case "boolean":
        {
          const port = this.getOneAvailablePort(Boolean(parma1));
          if (port === null) return null;
          url.port = String(port);
        }
        break;
      case "string":
        if (parma1 !== "") {
          const port = Number.parseInt(parma1);
          if (port > 0 && port < 65536) url.port = String(port);
        }
        break;
      case "number":
        if (parma1 > 0 && parma1 < 65535) url.port = String(parma1);
        break;
      // case 'object':
      default:
        break;
    }

    return url;
  }
}
